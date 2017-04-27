 #include "polygon.h"
polygon::polygon() { //set_side_length(0); }
polygon::~polygon() { }
void polygon::set_side_length(double side_length) { 
	printf("polygon side_length: %f\n\n",side_length); 
	side_length_ = side_length; 
	printf("polygon side_length_: %f\n\n",side_length_); 
}
double polygon::area() const { return 0; }
