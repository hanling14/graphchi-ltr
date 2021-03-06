/**
 * @file
 * @author  David Nemeskey
 * @version 0.1
 *
 * @section LICENSE
 *
 * Copyright [2013] [Carnegie Mellon University]
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * This is the entry point for the Learning to Rank toolkit. The user can
 * specify what input dataset he wants to use and what algorithm, and the
 * control is then forwarded to the selected algorithm.
 */
#include <string>

#include "ltr_common.hpp"
#include "input_formats.hpp"
#include "ranknet.hpp"
#include "ranknet_lambda.hpp"
#include "lambdarank.hpp"
#include "evaluation_measures.hpp"
#include "ml/learning_rate.h"
#include "ml/linear_regression.h"
#include "ml/neural_net.h"
#include "ml/regression_tree.h"

using namespace graphchi;

/**
 * Reads a dataset.
 * @return the number of shards.
 */
int read_data(std::string file_name, std::string file_type, size_t& dimensions) {
  if (file_type == "csv") {
    int qid_index = get_option_int("qid", 0);
    int doc_index = get_option_int("doc", 1);
    int rel_index = get_option_int("rel", -1);
    return read_csv(file_name, dimensions, qid_index, doc_index, rel_index);
  } else if (file_type == "letor") {
    return read_letor(file_name, dimensions);
  } else if (file_type == "yahoo") {
    return read_yahoo_ltr(file_name, dimensions);
  } else {
    return 0;
  } 
}

/** Instantiates the selected algorithm. */
LtrAlgorithm* get_algorithm(std::string name, DifferentiableModel* model,
                            EvaluationMeasure* eval, StoppingCondition stop) {
  if (name == "ranknet_old") {
    return new RankNet(model, eval, stop);
  } else if (name == "ranknet") {
    return new RankNetLambda(model, eval, stop);
  } else if (name == "lambdarank") {
    return new LambdaRank(model, eval, stop);
  } else {
    return NULL;
  }
}

/** Instantiates the ML model. */
DifferentiableModel* get_ml_model(const std::string& name, size_t dimensions, LearningRate* lr) {
  if (name == "linreg") {
    return new LinearRegression(dimensions, lr);
  } else if (name.compare(0, 2, "nn") == 0) {
    if (name.length() > 3) {
      int neurons = atoi(name.substr(3).c_str());
      if (neurons > 0) {
        return new NeuralNetwork(dimensions, neurons);
      }
    }
    std::cerr << "The number of neurons must be specified." << std::endl;
  }
  return NULL;
}

/**
 * Instantiates the evaluator object.
 * @param[in] cutoff the "at" in "nDCG@20".
 */
EvaluationMeasure* get_evaluation_measure(std::string name, int cutoff) {
  if (name == "ndcg") {
    return new NdcgEvaluator(cutoff);
  } else {
    return NULL;
  }
}

int main(int argc, const char ** argv) {
//  print_copyright();
//  NeuralNetwork(50, 20);

  /* GraphChi initialization will read the command line 
     arguments and the configuration file. */
  graphchi_init(argc, argv);

  /* Parameters */
  std::string train_data = get_option_string("train_data");  // TODO: not needed (save/load model)
  std::string eval_data = get_option_string("eval_data", "");
  std::string test_data = get_option_string("test_data", "");
  int niters            = get_option_int("niters", 10);
  int cutoff            = get_option_int("cutoff", 20);
  // TODO: make it overridable by --D?
  size_t dimensions     = 0;
  bool scheduler        = false;  // No scheduler is needed
  std::string reader       = get_option_string("reader");
  std::string error_metric = get_option_string("error", "ndcg");
  std::string model_name     = get_option_string("mlmodel", "linreg");
  std::string algorithm_name = get_option_string("algorithm", "ranknet");
  std::string learning_rate  = get_option_string("learning_rate", "");
  StoppingCondition stopping_condition =
      static_cast<StoppingCondition>(get_option_int("stopping_condition", 0));

  /* Read the data file. */
  int train_nshards = read_data(train_data, reader, dimensions);
  if (train_nshards == 0) {
    logstream(LOG_FATAL) << "Reader " << reader << " is not implemented. " <<
                            "Select one of csv, letor." << std::endl;
  }

  LearningRate* lr_obj = create_learning_rate_function(learning_rate);
  /* Instantiate the algorithm. */
  DifferentiableModel* model = get_ml_model(model_name, dimensions, lr_obj);
  if (model == NULL) {
    logstream(LOG_FATAL) << "Model " << model_name <<
                            " is not implemented; select one of " <<
                            "linreg, nn." << std::endl;
    exit(1);
  }
  EvaluationMeasure* eval = get_evaluation_measure(error_metric, cutoff);
  if (eval == NULL) {
    logstream(LOG_FATAL) << "Evaluation metric " << error_metric <<
                            " is not implemented; select one of " <<
                            "ndcg, err, map." << std::endl;
    exit(1);
  }
  LtrAlgorithm* algorithm = get_algorithm(algorithm_name, model,
                                          eval, stopping_condition);
  if (algorithm == NULL) {
    logstream(LOG_FATAL) << "Algorithm " << algorithm_name <<
                            " is not implemented; select one of " <<
                            "ranknet, lambdarank, lambdamart." << std::endl;
    exit(1);
  }

  /* Training. */
  metrics m_train("ltr_train");
  graphchi_engine<TypeVertex, FeatureEdge> engine(
      train_data, train_nshards, scheduler, m_train); 
  engine.run(*algorithm, niters);
  metrics_report(m_train);

  /* Validation. */
  if (eval_data != "") {
    int eval_nshards = read_data(eval_data, reader, dimensions);
    if (eval_nshards == 0) {
      logstream(LOG_FATAL) << "Reader " << reader << " is not implemented. " <<
                              "Select one of csv, letor." << std::endl;
    }
    algorithm->set_phase(VALIDATION);
    metrics m_eval("ltr_eval");
    graphchi_engine<TypeVertex, FeatureEdge> engine(
        eval_data, eval_nshards, scheduler, m_eval); 
    engine.run(*algorithm, niters);
    metrics_report(m_eval);
  }

  /* Testing. */
  if (test_data != "") {
    int test_nshards = read_data(test_data, reader, dimensions);
    if (test_nshards == 0) {
      logstream(LOG_FATAL) << "Reader " << reader << " is not implemented. " <<
                              "Select one of csv, letor." << std::endl;
    }
    algorithm->set_phase(TESTING);
    metrics m_test("ltr_test");
    graphchi_engine<TypeVertex, FeatureEdge> engine(
        test_data, test_nshards, scheduler, m_test); 
    engine.run(*algorithm, niters);
    metrics_report(m_test);
  }

  return 0;
}

/*
 * Problems:
 * - MlModel: must have a save/load function
 */

