#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <functional>
#include <vector>
#include <map>
#include <queue>
#include <set>

enum class Side {
    buy = 0,
    sell = 1
};
enum class ordtype {
    limit  = 'l',
    market = 'm',
    stop   = 's',
    cancel = 'c'
};
enum class side {
    buy  = 'b',
    sell = 's',
    none = 'n'
};
using ordid = int;
using price = long;  // the price contains 2 fixed decimal, so we 
                     // multiply all price with 100 to avoid float computing 
struct compare_factor {price p;ordid o;};  // price, ordid
using cmpfct = struct compare_factor;

using volume = long;
using type = char;

class Order {
public:
    explicit Order(ordtype t = ordtype::limit, 
                   side s    = side::buy,
                   volume v  = 0L, 
                   price  p  = 0L) noexcept:
        _o(_id++), _t(t), _s(s), _v(v), _p(p){}
    friend std::ostream& operator << (std::ostream& os, const Order& o);
    virtual void execute(OpenBooks&){};
    virtual void deposit(){};
private:
    static int   _id;
    const ordid   _o;             // order number 
    const ordtype _t;
    const side    _s; 
    const volume  _v;
    const price   _p;
};
int Order::_id = 1;
class LimitOrder:public Order{
public:
    LimitOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual void execute(OpenBooks&); 
    virtual void deposit(){};
};

class MarketOrder:public Order{
public:
    MarketOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual void execute(OpenBooks&){std::cout<<"m\n";};
    virtual void deposit(){};
};
class StopOrder:public Order{
public:
    StopOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual void execute(OpenBooks&){std::cout<<"s\n";};
    virtual void deposit(){};
};
class CancelOrder:public Order{
public:
    CancelOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual void execute(OpenBooks&){std::cout<<"c\n";};
    virtual void deposit(){};
};
LimitOrder::execute(OpenBooks& ob) {
         
}
class OpenBooks {
public:
    OpenBooks();
    bool recvOrder(ordtype, side, volume, price);
    bool processOrder();
private:
    // std::function<bool(const cmpfct l, const cmpfct r)> _cmp[2];
    std::function<bool(const cmpfct l, const cmpfct r)> _cmp_s; 
    std::function<bool(const cmpfct l, const cmpfct r)> _cmp_b; 

    int buy = static_cast<int>(Side::buy); 
    int sell= static_cast<int>(Side::sell);

    std::priority_queue<cmpfct, std::vector<cmpfct>, 
        decltype(_cmp_b)>  _limit_order_b;
    std::priority_queue<cmpfct, std::vector<cmpfct>, 
        decltype(_cmp_s)> _limit_order_s;

    std::vector<Order>                _orders;
    std::map<price, ordid>            _stop_order  [2];
    std::set<ordid>                   _cancel_order[2];
    std::vector<ordid>                _market_order[2];
    Order *                           _pOrder;
    Order* _createOrder(ordtype, side, volume, price);
};

OpenBooks::OpenBooks() {
    _pOrder = nullptr;
    _orders.reserve(64);
    int buy = static_cast<int>(Side::buy);
    int sell= static_cast<int>(Side::sell);

    //_cmp[buy]  = [](cmpfct l, cmpfct r){
    _cmp_b = [](cmpfct l, cmpfct r){
        return l.p<r.p || l.p==r.p && l.o>r.o;};
    //_cmp[sell] = [](cmpfct l, cmpfct r){ 
    _cmp_s = [](cmpfct l, cmpfct r){ 
        return l.p>r.p || l.p==r.p && l.o>r.o;};
    for(auto v:_market_order) v.reserve(64);
}

bool OpenBooks::recvOrder(ordtype t, side s, volume v, price p) {
    _pOrder = _createOrder(t, s, v, p);
    if (_pOrder==nullptr) return false;
    _orders.push_back(*_pOrder);  // deposit
    return true;
}

bool OpenBooks::processOrder() {
    if (_pOrder==nullptr) return false;
    _pOrder.execute(*this); 
}
Order* OpenBooks::_createOrder(ordtype t, side s, volume v, price p) {
    switch (t) {
        case ordtype::limit :return new LimitOrder (t, s, v, p);
        case ordtype::stop  :return new StopOrder  (t, s, v, p);
        case ordtype::cancel:return new CancelOrder(t, s, v, p);
        case ordtype::market:return new MarketOrder(t, s, v, p);
    }
    return nullptr; 
}

inline char* const side2str(side s) {
    switch (s){
        case side::buy:  return (char*)"buy";
        case side::sell: return (char*)"sell";
    }
    return (char*)"na";
}

inline char* const ordtype2str(ordtype t) {
    switch (t){
        case ordtype::limit: return (char*)"limit";
        case ordtype::market:return (char*)"market";
        case ordtype::stop:  return (char*)"stop";
        case ordtype::cancel:return (char*)"cancel";
    }
    return (char*)"na";
}

std::ostream& operator<<(std::ostream& os, const Order& o) {
    os<<o._o<<" " << ordtype2str(o._t)<<" "<<side2str(o._s)<<" "
      <<o._v<<" "<<(long)(o._p/100) << "."
      << std::setfill('0') << std::setw(2) << (int)(o._p%100);
    return os;
}

bool parseLine(const std::string & line, OpenBooks & exch)
{
    char ordtyp[8]  = {0x0};
    char sidestr[8] = {0x0};
    long vol        = 0L;
    long iprice     = 0L; // integer part of price
    int  dprice     = 0L; // decimal part of price
    
    int narg = std::sscanf(line.c_str(),"%6s %4s %ld %ld.%d", 
             ordtyp, sidestr, &vol, &iprice, &dprice);
    if (narg != 5) {
        return false;
    }
    if (!exch.recvOrder(
             static_cast<ordtype>(ordtyp[0]), 
             static_cast<side>(sidestr[0]), 
             vol, 
             iprice*100+dprice))  // to avoid floating operation 
        std::cerr << "error when recvOrder"<<std::endl;
    if (!exch.processOrder())
        std::cerr << "error when processOrder"<<std::endl;
   
}

int main(int argc, char * argv[])
{
    OpenBooks exch;
    std::ifstream inf("odbk_test1");
    char line[100];
    //std::string line;
    // while (std::getline(std::cin, line))
    while(!inf.getline(line, 100).eof())
    {
        if (!parseLine(line, exch))
        {
            std::cerr << "Error while parsing line \"" 
                      << line << "\"" << std::endl;
            return 1;
        }
    }
    // print
    /*
    for(auto v : exch._orders) {
        std::cout<<v<<std::endl;
    }*/ 
    return 0;
}
