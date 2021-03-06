#include "ml/neural_net_activation.h"

#include <cmath>

ActivationAct::ActivationAct(const Activation& parent) : parent_(parent) {}
ActivationDeriv::ActivationDeriv(const Activation& parent) : parent_(parent) {}

Activation::Activation() : act_(new ActivationAct(*this)),
                           deriv_(new ActivationDeriv(*this)) {}

Activation::Activation(Activation& orig) : act_(new ActivationAct(*this)),
                                           deriv_(new ActivationDeriv(*this)) {}

Activation::~Activation() {
  delete act_;
  delete deriv_;
}

Sigma::Sigma(double K) : K(K) {}

Sigma* Sigma::clone() {
  return new Sigma(*this);
}

double Sigma::activation(double x) const {
  return 1 / (1 + exp(-K * x));
}

double Sigma::derivative(double sigma_x) const {
//  double sigma_x = *this(x);
  return sigma_x * (1 - sigma_x);  // TODO: +epsilon(0.1) to avoid saturation?
}

double Sigma::logit(double x) const {
  return log(x) - log(1 - x);
}
