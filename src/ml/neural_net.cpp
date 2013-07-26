#include "ml/neural_net.h"

NeuralNetwork::NeuralNetwork(size_t dimensions, size_t hidden_neurons,
                             double learning_rate)
    : MlModel(dimensions, learning_rate), K(1) {
  initialize_weights(hidden_neurons);
  outputs.resize(hidden_neurons);
}

double NeuralNetwork::score(double* const& features) const {
  return score_inner(features, outputs);
}

double NeuralNetwork::score_inner(double* const& features,
                                  std::vector<double> outputs1) const {
  std::fill(outputs1.begin(), outputs1.end(), 0);
  for (size_t x = 0; x < dimensions; x++) {
    for (size_t h = 0; h < outputs1.size(); h++) {
      outputs1[h] += features[x] * w1[x][h];
    }
  }
  for (size_t i = 0; i < outputs1.size(); i++) {
    outputs1[i] = sigma(outputs1[i]);
  }

  double y = 0;
  for (size_t h = 0; h < outputs1.size(); h++) {
    y += outputs1[h] * wy[h];
  }
  y = sigma(y);

  return y;
}

/** The sigma function... */
double NeuralNetwork::sigma(double x) const {
  return 1 / (1 + exp(-K * x));
}
/** ... and its derivative. */
double NeuralNetwork::sigma_deriv(double x) const {
  double fx = sigma(x);
  return fx * (1 - fx);
}
/** The inverse of the sigma (logistic) function. */
double NeuralNetwork::logit(double x) const {
  return log(x) - log(1 - x);
}

void NeuralNetwork::initialize_weights(size_t hidden_neurons) {
  std::uniform_real_distribution<double> unif(0.1, 1.0);
  std::default_random_engine re;
  re.seed(1001);

  w1.resize(dimensions);  // TODO: +1 noise
  for (size_t i = 0; i < dimensions; i++) {
    w1[i].resize(hidden_neurons);
    for (size_t j = 0; j < hidden_neurons; j++) {
      w1[i][j] = unif(re);
    }
  }

  wy.resize(hidden_neurons);
  for (size_t i = 0; i < hidden_neurons; wy[i++] = unif(re));
}

NeuralNetworkGradient::NeuralNetworkGradient(NeuralNetwork& parent)
    : parent(parent) {
  outputs.resize(parent.outputs.size());
  gradients1.resize(parent.w1.size());
  for (size_t i = 0; i < parent.w1.size(); i++) {
    gradients1[i].resize(parent.w1[i].size());
  }
  gradientsy.resize(parent.wy.size());
  reset();
}

void NeuralNetworkGradient::reset() {
  for (size_t i = 0; i < gradients1.size(); i++) {
    for (size_t j = 0; j < gradients1[i].size(); j++) {
      gradients1[i][j] = 0;
    }
  }
  for (size_t i = 0; i < gradientsy.size(); i++) {
    gradientsy[i] = 0;
  }
}

void NeuralNetworkGradient::update(double* const& features,
                                   double y, double mult) {
  /* Have to run score() again to fill up the outputs vector... */
  parent.score_inner(features, outputs);

  /* First, let's update the output layer. */
  double deltay = y * (1 - y);
  for (size_t j = 0; j < parent.wy.size(); j++) {
    /* sgm'(s) * d(s) / d(w_j). */
    gradientsy[j] -= parent.learning_rate * mult * deltay * outputs[j];
  }

  /* That was the easy part; now the hidden layer... */
  for (size_t h = 0; h < parent.wy.size(); h++) {
    double deltah = outputs[h] * (1 - outputs[h]);
    for (size_t i = 0; i < gradients1.size(); i++) {
      gradients1[i][h] -= parent.learning_rate * mult *
                          deltay * parent.wy[h] * deltah * features[i];
    }
  }
}

void NeuralNetworkGradient::update_parent() {
  // TODO: -=?
  for (size_t i = 0; i < gradients1.size(); i++) {
    for (size_t j = 0; j < gradients1[i].size(); j++) {
      parent.w1[i][j] -= gradients1[i][j];
    }
  }
  for (size_t i = 0; i < gradientsy.size(); i++) {
    parent.wy[i] -= gradientsy[i];
  }
}

