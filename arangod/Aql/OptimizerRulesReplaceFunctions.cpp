////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRules.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/SortNode.h"
#include "Aql/TraversalNode.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/SmallVector.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Indexes/Index.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

//NEAR(coll, 0 /*lat*/, 0 /*lon*/[, 10 /*limit*/])
struct NearOrWithinParams{
  std::string collection;
  AstNode* latitude = nullptr;
  AstNode* longitude = nullptr;
  AstNode* limit = nullptr;
  AstNode* radius = nullptr;
  AstNode* distanceName = nullptr;

  NearOrWithinParams(AstNode const* node, bool isNear){
    TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_FCALL);
    AstNode* arr = node->getMember(0);
    TRI_ASSERT(arr->type == AstNodeType::NODE_TYPE_ARRAY);
    if (arr->getMember(0)->isStringValue()){
      collection = arr->getMember(0)->getString();
      // otherwise the "" collection will not be found
    }
    latitude = arr->getMember(1);
    longitude = arr->getMember(2);
    if(arr->numMembers() > 4){
      distanceName = arr->getMember(4);
    }

    if(isNear){
      limit = arr->getMember(3);
    } else {
      radius = arr->getMember(3);
    }
  }
};

//FULLTEXT(collection, "attribute", "search", 100 /*limit*/[, "distance name"])
struct FulltextParams{
  std::string collection;
  std::string attribute;
  AstNode* limit = nullptr;

  FulltextParams(AstNode const* node){
    TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_FCALL);
    AstNode* arr = node->getMember(0);
    TRI_ASSERT(arr->type == AstNodeType::NODE_TYPE_ARRAY);
    if (arr->getMember(0)->isStringValue()){
      collection = arr->getMember(0)->getString();
    }
    if (arr->getMember(1)->isStringValue()){
      attribute = arr->getMember(1)->getString();
    }
    if(arr->numMembers() > 3){
      limit = arr->getMember(3);
    }
  }
};

AstNode* getAstNode(CalculationNode* c){
  return c->expression()->nodeForModification();
}

Function* getFunction(AstNode const* ast){
  if (ast->type == AstNodeType::NODE_TYPE_FCALL){
    return static_cast<Function*>(ast->getData());
  }
  return nullptr;
}

AstNode* createSubqueryWithLimit(
  ExecutionPlan* plan,
  ExecutionNode* node,
  ExecutionNode* first,
  ExecutionNode* last,
  Variable* lastOutVariable,
  AstNode* limit
  ){
  // Creates a subquery of the following form:
  //
  //    singleton
  //        |
  //      first
  //        |
  //       ...
  //        |
  //       last
  //        |
  //     [limit]
  //        |
  //      return
  //
  // The subquery is then injected into the plan before the given `node`
  // This function returns an `AstNode*` of type reference to the subquery's
  // `outVariable` that can be used to replace the expression (or only a
  // part) of a `CalculationNode`.
  //
  if(limit && !(limit->isIntValue() || limit->isNullValue())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,"limit parameter is for wrong type");
  }


  auto* ast = plan->getAst();

  /// singleton
  ExecutionNode* eSingleton = plan->registerNode(
      new SingletonNode(plan, plan->nextId())
  );

  /// return
  ExecutionNode* eReturn = plan->registerNode(
      // link output of index with the return node
      new ReturnNode(plan, plan->nextId(), lastOutVariable)
  );

  /// link nodes together
  first->addDependency(eSingleton);
  eReturn->addDependency(last);

  /// add optional limit node
  if(limit && !limit->isNullValue()) {
    ExecutionNode* eLimit = plan->registerNode(
      new LimitNode(plan, plan->nextId(), 0 /*offset*/, limit->getIntValue())
    );
    plan->insertAfter(last, eLimit); // inject into plan
  }

  /// create subquery
  Variable* subqueryOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eSubquery = plan->registerSubquery(
      new SubqueryNode(plan, plan->nextId(), eReturn, subqueryOutVariable)
  );

  plan->insertBefore(node, eSubquery);

  // return reference to outVariable
  return ast->createNodeReference(subqueryOutVariable);

}

AstNode* replaceNearOrWithin(AstNode* funAstNode, ExecutionNode* calcNode, ExecutionPlan* plan, bool isNear){
  auto* ast = plan->getAst();
  auto* query = ast->query();
  auto* trx = query->trx();
  NearOrWithinParams params(funAstNode,isNear);

  // RETURN (
  //  FOR d IN col
  //    SORT DISTANCE(d.lat, d.long, param.lat, param.lon) // NEAR
  //    // FILTER DISTANCE(d.lat, d.long, param.lat, param.lon) < param.radius //WHITHIN
  //    MERGE(d, { param.distname : DISTANCE(d.lat, d.long, param.lat, param.lon)})
  //    LIMIT param.limit // NEAR
  //    RETURN d MERGE {param.distname : calculated_distance}
  // )

  //// enumerate collection
  auto* aqlCollection = aql::addCollectionToQuery(query, params.collection, false);
  if(!aqlCollection) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,"collection used in NEAR or WITHIN not found");
  }

  Variable* enumerateOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eEnumerate = plan->registerNode(
      // link output of index with the return node
      new EnumerateCollectionNode(
            plan, plan->nextId(), aqlCollection,
            enumerateOutVariable, false
      )
  );

  //// build sort condition - DISTANCE(d.lat, d.long, param.lat, param.lon)
  auto* docRef = ast->createNodeReference(enumerateOutVariable);
  AstNode* accessNodeLat = docRef;
  AstNode* accessNodeLon = docRef;
  bool indexFound = false;

  // figure out index to use
	std::vector<basics::AttributeName> field;
	auto indexes = trx->indexesForCollection(params.collection);
	for(auto& idx : indexes){
		if(Index::isGeoIndex(idx->type())) {
      // we take the first index that is found

      bool isGeo1 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX;
      bool isGeo2 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX;
      bool isGeo = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;

      auto fieldNum = idx->fields().size();
      if ((isGeo2 || isGeo) && fieldNum == 2) { // individual fields
        auto accessLatitude = idx->fields()[0];
        auto accessLongitude  = idx->fields()[1];

        accessNodeLat = ast->createNodeAttributeAccess(accessNodeLat, accessLatitude);
        accessNodeLon = ast->createNodeAttributeAccess(accessNodeLon, accessLongitude);

        indexFound = true;
      } else if ((isGeo1 || isGeo) && fieldNum == 1) {
        auto accessBase = idx->fields()[0];
        AstNode * base = ast->createNodeAttributeAccess(accessNodeLon, accessBase);

        VPackBuilder builder;
        idx->toVelocyPack(builder,true,false);
        bool geoJson = basics::VelocyPackHelper::getBooleanValue(builder.slice(), "geoJson", false);

        accessNodeLat = ast->createNodeIndexedAccess(base, ast->createNodeValueInt(geoJson ? 1 : 0));
        accessNodeLon = ast->createNodeIndexedAccess(base, ast->createNodeValueInt(geoJson ? 0 : 1));
        indexFound = true;
      }
      break;
    }

  } // for index in collection


  if(!indexFound) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_GEO_INDEX_MISSING);
  }

  auto* argsArray = ast->createNodeArray();
  argsArray->addMember(accessNodeLat);
  argsArray->addMember(accessNodeLon);
  argsArray->addMember(params.latitude);
  argsArray->addMember(params.longitude);
  auto* funDist = ast->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("DISTANCE"), argsArray);

  AstNode* expressionAst = funDist;

  //// build filter condition for
  if(!isNear){
    if(!params.radius->isNumericValue()){
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,"radius argument is not a numeric value");
    }

    expressionAst = ast->createNodeBinaryOperator(
      AstNodeType::NODE_TYPE_OPERATOR_BINARY_LE, funDist ,params.radius
    );
  }

  //// create calculation node used in SORT or FILTER
  //Calculation Node will acquire ownership
  Expression* calcExpr = new Expression(plan, ast, expressionAst);

  // put condition into calculation node
  Variable* calcOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eCalc = plan->registerNode(
      new CalculationNode(plan, plan->nextId(), calcExpr, nullptr, calcOutVariable)
  );
  eCalc->addDependency(eEnumerate);

  //// create SORT of FILTER
  ExecutionNode* eSortOrFilter = nullptr;
  if(isNear){
    // use calculation node in sort node
    SortElementVector sortElements { SortElement{ calcOutVariable, /*asc*/ true} };
    eSortOrFilter = plan->registerNode(
        new SortNode(plan, plan->nextId(), sortElements, false)
    );
  } else {
    eSortOrFilter = plan->registerNode(
        new FilterNode(plan, plan->nextId(), calcOutVariable)
    );
  }
  eSortOrFilter->addDependency(eCalc);

  //// create MERGE(d, { param.distname : DISTANCE(d.lat, d.long, param.lat, param.lon)})
  if(params.distanceName) { //return without merging the distance into the result
    if(!params.distanceName->isStringValue()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,"distance argument is not a string");
    }
    AstNode* elem = nullptr;
    AstNode* funDistMerge = nullptr;
    if(isNear){
      funDistMerge = ast->createNodeReference(calcOutVariable);
    } else {
      //NOTE - recycling the Ast seems to work - tested with ASAN
      funDistMerge = funDist;
    }
    if(params.distanceName->isConstant()){
      elem = ast->createNodeObjectElement(params.distanceName->getStringValue()
                                         ,params.distanceName->getStringLength()
                                         ,funDistMerge);
    } else {
      elem = ast->createNodeCalculatedObjectElement(params.distanceName, funDistMerge );
    }
    auto* obj = ast->createNodeObject();
    obj->addMember(elem);

    auto* argsArrayMerge = ast->createNodeArray();
    argsArrayMerge->addMember(docRef);
    argsArrayMerge->addMember(obj);

    auto* funMerge = ast->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("MERGE"), argsArrayMerge);

    Variable* calcMergeOutVariable = ast->variables()->createTemporaryVariable();
    Expression* calcMergeExpr = new Expression(plan, ast, funMerge);
    ExecutionNode* eCalcMerge = plan->registerNode(
      new CalculationNode(plan, plan->nextId(), calcMergeExpr, nullptr, calcMergeOutVariable)
    );
    plan->insertAfter(eSortOrFilter, eCalcMerge);

    //// wrap plan part into subquery
    return createSubqueryWithLimit(plan, calcNode, eEnumerate, eCalcMerge, calcMergeOutVariable, params.limit);
  }

  //// wrap plan part into subquery
  return createSubqueryWithLimit(plan, calcNode,
                                 eEnumerate /* first */, eSortOrFilter /* last */,
                                 enumerateOutVariable, params.limit);
}

AstNode* replaceFullText(AstNode* funAstNode, ExecutionNode* calcNode, ExecutionPlan* plan){
  auto* ast = plan->getAst();
  auto* query = ast->query();
  auto* trx = query->trx();

  FulltextParams params(funAstNode); // must be NODE_TYPE_FCALL

  /// index
  //  we create this first as creation of this node is more
  //  likely to fail than the creation of other nodes

  //  index - part 1 - figure out index to use
  std::shared_ptr<arangodb::Index> index = nullptr;
	std::vector<basics::AttributeName> field;
	TRI_ParseAttributeString(params.attribute, field, false);
	auto indexes = trx->indexesForCollection(params.collection);
	for(auto& idx : indexes){
		if(idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX) {
			if(basics::AttributeName::isIdentical(idx->fields()[0], field, false /*ignore expansion in last?!*/)) {
				index = idx;
				break;
			}
		}
	}

	if(!index){ // not found or error
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING);
	}

  // index part 2 - get remaining vars required for index creation
  auto* aqlCollection = aql::addCollectionToQuery(query, params.collection, false);
  if(!aqlCollection) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,"collection used in FULLTEXT not found");
  }
	auto condition = std::make_unique<Condition>(ast);
	condition->andCombine(funAstNode);
  condition->normalize(plan);
	// create a fresh out variable
  Variable* indexOutVariable = ast->variables()->createTemporaryVariable();

  ExecutionNode* eIndex = plan->registerNode(
    new IndexNode(
      plan,
      plan->nextId(),
      aqlCollection,
      indexOutVariable,
      std::vector<transaction::Methods::IndexHandle> {
				transaction::Methods::IndexHandle{index}
      },
      std::move(condition),
      IndexIteratorOptions()
    )
  );

  //// wrap plan part into subquery
  return createSubqueryWithLimit(plan, calcNode, eIndex, eIndex, indexOutVariable, params.limit);
}

} // namespace



//! @brief replace legacy JS Functions with pure AQL
void arangodb::aql::replaceNearWithinFulltext(Optimizer* opt
                                             ,std::unique_ptr<ExecutionPlan> plan
                                             ,OptimizerRule const* rule){

  bool modified = false;

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for(auto const& node : nodes){
    auto visitor = [&modified, &node, &plan](AstNode* astnode){
      auto* fun = getFunction(astnode); // if fun != nullptr -> astnode->type NODE_TYPE_FCALL
      AstNode* replacement = nullptr;
      if(fun){
        if (fun->name == "NEAR"){
          replacement = replaceNearOrWithin(astnode, node, plan.get(), true /*isNear*/);
        } else if (fun->name == "WITHIN"){
          replacement = replaceNearOrWithin(astnode, node, plan.get(), false /*isNear*/);
        } else if (fun->name == "FULLTEXT"){
          replacement = replaceFullText(astnode, node,plan.get());
        }
      }
      if (replacement) {
        modified = true;
        return replacement;
      }
      return astnode;
    };

    CalculationNode* calc = static_cast<CalculationNode*>(node);
    auto* original = getAstNode(calc);
    auto* replacement = Ast::traverseAndModify(original,visitor);

    // replace root node if it was modified
    // TraverseAndModify has no access to roots parent
    if (replacement != original) {
      calc->expression()->replaceNode(replacement);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);

}; // replaceJSFunctions
