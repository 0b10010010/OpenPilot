/* $Id$ */

#include <cmath>

#include "kernel/jafarException.hpp"
#include "kernel/jafarDebug.hpp"

#include "jmath/random.hpp"

using namespace jblas;

using namespace jafar::jmath;

/*
 * class UniformDistribution
 */

UniformDistribution::UniformDistribution(double min_, double max_,
                                         unsigned int seed_) :
  rng(seed_),
  uniform(min_, max_),
  vg(rng, uniform) {}

UniformDistribution::~UniformDistribution() {}

double UniformDistribution::get() {
  return vg();
}

/*
 * class MultiDimUniformDistribution
 */

MultiDimUniformDistribution::MultiDimUniformDistribution(std::size_t size_, unsigned int seed_) :
    u(size_),
    uniformsVec(size_) 
{
  for (std::size_t i =0 ; i <size_ ; i++) {
    uniformsVec[i] = new  UniformDistribution(seed_+i);
  } 
}

MultiDimUniformDistribution::MultiDimUniformDistribution(const vec& min_, const vec& max_, 
							 unsigned int seed_) :
  u(min_.size()),
  uniformsVec(min_.size())
{
  JFR_PRECOND(min_.size() == max_.size(),
              "MultiDimUniformDistribution::MultiDimUniformDistribution: min_ and max_ size must be equal");

  for (std::size_t i =0 ; i <uniformsVec.size() ; i++) {
    uniformsVec[i] = new  UniformDistribution(min_(i), max_(i), seed_+i);
  }
}

MultiDimUniformDistribution::~MultiDimUniformDistribution() {
  for (std::size_t i =0 ; i <uniformsVec.size() ; i++) {
    delete uniformsVec[i];
  }
}

vec& MultiDimUniformDistribution::get() {
  for (std::size_t i=0 ; i <u.size() ; i++) {
    u(i) = uniformsVec[i]->get();
  }
  return u;
}

/*
 * class NormalDistribution
 */

NormalDistribution::NormalDistribution(double mean_, double sigma_, 
                                       unsigned int seed_) :
  rng(seed_),
  normal(mean_, sigma_),
  vg(rng, normal) 
{
  get();
}

NormalDistribution::~NormalDistribution() {}

double NormalDistribution::get() {
  return vg();
}

/*
 * class MultiDimNormalDistribution
 */

MultiDimNormalDistribution::MultiDimNormalDistribution(std::size_t size_, unsigned int seed_) :
  normalsVec(size_)
{
  for (std::size_t i=0 ; i <size_ ; i++) {
    normalsVec[i] = new  NormalDistribution(seed_+i);
  }
}

MultiDimNormalDistribution::MultiDimNormalDistribution(const vec& mean_, const vec& cov_, 
						       unsigned int seed_) :
  normalsVec(mean_.size())
{
  JFR_PRECOND(cov_.size() == mean_.size(),
              "MultiDimNormalDistribution::MultiDimNormalDistribution: size of cov_ does not match");
  for (std::size_t i=0 ; i <normalsVec.size() ; i++) {
    normalsVec[i] = new  NormalDistribution(mean_(i), sqrt(cov_(i)), seed_+i);
  }
}

MultiDimNormalDistribution::~MultiDimNormalDistribution() {
  for (std::size_t i=0 ; i <normalsVec.size() ; i++) {
    delete normalsVec[i];
  }
}


vec MultiDimNormalDistribution::get() {
  vec u(normalsVec.size());
  for (std::size_t i =0 ; i < normalsVec.size() ; i++) {
    u(i) = normalsVec[i]->get();
  }
  return u;
}
