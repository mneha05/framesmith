#include "FramePacer.hpp"
#include <algorithm>
#include <vector>
void FramePacer::push(double ms){ samples_[cursor_++%samples_.size()]=ms; count_=std::min(count_+1,samples_.size()); }
double FramePacer::average() const { if(!count_)return 0; double s=0; for(size_t i=0;i<count_;++i)s+=samples_[i]; return s/count_; }
double FramePacer::p95() const { if(!count_)return 0; std::vector<double> v(samples_.begin(),samples_.begin()+count_); std::sort(v.begin(),v.end()); return v[std::min(v.size()-1,(size_t)(0.95*(v.size()-1)))]; }
bool FramePacer::janky(double budget_ms) const { return p95()>budget_ms*1.10; }
