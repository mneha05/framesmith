#include "../app/src/main/cpp/FramePacer.hpp"
#include <cassert>
#include <iostream>
int main(){ FramePacer p; for(int i=0;i<100;i++)p.push(8.0+(i%5)); assert(p.average()>8 && p.average()<13); assert(!p.janky()); for(int i=0;i<100;i++)p.push(25); assert(p.janky()); std::cout<<"PASS avg="<<p.average()<<" p95="<<p.p95()<<"
"; }
