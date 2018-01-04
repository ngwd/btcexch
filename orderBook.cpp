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
using ordid  = int;
using price  = long;  
using volume = long;
using type   = char;
struct compare_factor {price p;ordid o;};  // price, ordid
using cmpfct = struct compare_factor;
struct match_record   {ordid taker; ordid maker; volume v; price p;};
class OpenBooks;
class Order {
public:
    explicit Order(ordtype t = ordtype::limit, 
                   side s    = side::buy,
                   volume v  = 0L, 
                   price  p  = 0L) noexcept:
        _o(_id++), _t(t), _s(s), _v(v), _p(p){}
    inline ordid   getOrdid()    const {return _o;}
    inline volume  getQuantity() const {return _v;}
    inline price   getPrice()    const {return _p;}
    inline ordtype getType()     const {return _t;}
    inline void    setQuantity(volume v) {_v =v;}
    virtual bool   canMatch(OpenBooks&){}
    virtual void   transform(){}
    friend std::ostream& operator << (std::ostream& os, const Order& o);
private:
    static int   _id;
    const   ordid   _o;             // order number 
protected:
    mutable ordtype _t;
    mutable price   _p;
    const   side    _s; 
    mutable volume  _v;
    side _getOpposingSide() const; 
};
int Order::_id = 1;

class OpenBooks {
public:
    OpenBooks();
    bool   recvOrder(ordtype, side, volume, price);
    void   processOrder();
    price  getMarketPrice(side)const;
    bool   marketExist(side) const ;
private:
    // std::function<bool(const cmpfct l, const cmpfct r)> _cmp[2];
    std::function<bool(const cmpfct l, const cmpfct r)> _cmp_s; 
    std::function<bool(const cmpfct l, const cmpfct r)> _cmp_b; 

    side _s;  // current order process is buy side or sell side
    std::priority_queue<cmpfct, std::vector<cmpfct>, 
        decltype(_cmp_b)>  _limit_order_b;
    std::priority_queue<cmpfct, std::vector<cmpfct>, 
        decltype(_cmp_s)> _limit_order_s;

    std::vector<Order>                _orders;
    std::map<price, ordid>            _stop_order  [2];
    std::set<ordid>                   _cancel_order;
    std::vector<ordid>                _market_order[2];
    Order *                           _pOrder;
    std::vector<match_record>         _matches;

    Order* _createOrder(ordtype, side, volume, price);
    void   _execute(Order*, ordtype t);
};

side Order::_getOpposingSide() const {
    if (_s==side::none) 
        return _s;
    else 
        return _s==side::buy?side::sell:side::buy;
}
class LimitOrder:public Order{
public:
    LimitOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual bool canMatch(OpenBooks&);
};

class MarketOrder:public Order{
public:
    MarketOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual bool canMatch(OpenBooks&);
};
class StopOrder:public Order{
public:
    StopOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual bool canMatch(OpenBooks&);
    virtual void transform(){
        _t = ordtype::market;
        _p = 0;
    }
};
class CancelOrder:public Order{
public:
    CancelOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
};

bool LimitOrder::canMatch(OpenBooks& ob) {
    side s = _getOpposingSide();
    price opp_p = ob.getMarketPrice(s);
    return _v > 0 &&    // volume should > 0 
           ob.marketExist(s) &&  
          (_s==side::sell && _p<=ob.getMarketPrice(s)|| 
           _s==side::buy  && _p>=ob.getMarketPrice(s));
}
bool MarketOrder::canMatch(OpenBooks& ob) {
    side s = _getOpposingSide();
    return _v > 0 &&    // volume should > 0 
           !ob.marketExist(s);  
}
bool StopOrder::canMatch(OpenBooks& ob) {
    side s = _getOpposingSide();
    return _v > 0 &&
           ob.marketExist(s) && 
           (_s==side::sell && _p>=ob.getMarketPrice(s)||
            _s==side::buy  && _p<=ob.getMarketPrice(s));
}

OpenBooks::OpenBooks() {
    _pOrder = nullptr;
    _orders.reserve(64);
    _matches.reserve(64);

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
    _s = s;                       // buy side or sell side
    return true;
}

void OpenBooks::processOrder() {
    ordtype t = _pOrder->getType(); 
    switch(t) {
    case ordtype::limit:
    case ordtype::market:
        while (_pOrder->canMatch(*this)) // continuous matching
            _execute(_pOrder, t); 
        if (_pOrder->getQuantity()) {
            //_deposit(_pOrder);
        }
        break;
    case ordtype::stop:
        if (_pOrder->canMatch(*this)) { 
            _pOrder->transform();  // transform to market order
            this->processOrder();
        } 
        break;
    case ordtype::cancel:
        _execute(_pOrder, t);
        break;
    }
}
void OpenBooks::_execute(Order* pOrder, ordtype t) {
    switch(t) {
        case ordtype::limit:
        case ordtype::market:
        {
            auto& pq; //priority queue
            if (_s==side::buy) pq = &_limit_order_s;
            else pq = &_limit_order_b;

            ordid matched_o = pq.top().o;

            if (_cancel_order.find(matched_o)!=_cancel_order.end()){
                // this order should not have been canceled
                pq.pop();
                matched_o = pq.top().o;
            }
            price  matched_pri = _orders[matched_o].getPrice();
            volume matched_qty = _orders[matched_o].getQuantity(); 
            volume qty = pOrder->getQuantity();
            if (matched_qty > qty){
                _matches.push_back({pOrder->getOrdid(), matched_o, 
                                    qty, matched_pri}); 
                pOrder->setQuantity(0);
                _orders[matched_o].setQuantity(matched_qty-qty);
            }
            else {
                _matches.push_back({pOrder->getOrdid(), matched_o, 
                                    matched_qty, matched_pri}); 
                pOrder->setQuantity(qty-matched_qty);
                pq.pop();
            }
        }
        break;
        case ordtype::cancel:
        { 
            ordid co = _pOrder->getQuantity();//for cancel order, this is 
                                              // the order will be canceled
            if (_pOrder->getOrdid()<co) {
                std::cerr << "error cancel unexist order"<<std::endl;
                return;
            }
            _cancel_order.insert(co);
        }
        break;
        case ordtype::stop:
        break;
    }
}
price OpenBooks::getMarketPrice(side s)const {
    if (s==side::buy) auto pq = &_limit_order_b;
    else auto pq = &_limit_order_s;
    return pq.top().p;
}
bool OpenBooks::marketExist(side s) const {
    if (s==side::buy) auto pq = &_limit_order_b;
    else auto pq = &_limit_order_s;
    return !pq.empty();
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

bool parseLine(const std::string & line, OpenBooks & exch) {
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
    exch.processOrder();
    return true;
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
