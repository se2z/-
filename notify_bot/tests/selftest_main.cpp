#include <iostream>
namespace notify::tests { void run(); }
int main(){try{notify::tests::run();std::cout<<"pure tests passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
