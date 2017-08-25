#include "polygon.hpp"
#include <cmath>

class triangle : public polygon {
public:
    virtual double area();//{
    //    return side_length_ * side_length_ * sqrt(3) / 2;
    //}
};


// the class factories
double triangle::area(){
    return side_length_ * side_length_ * sqrt(3) / 2;
}
extern "C" polygon* create() {
    return new triangle;
}

extern "C" void destroy(polygon* p) {
    delete p;
}
