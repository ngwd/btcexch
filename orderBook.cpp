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

struct compare_factor {price p;volume v;ordid o;};  // price, quantity ordid
using cmpfct = struct compare_factor;
using cmpfunc = std::function<bool(const cmpfct l, const cmpfct r)>;
using pqueue = std::priority_queue<cmpfct, std::vector<cmpfct>, cmpfunc> ;

struct match_record   {ordid taker; ordid maker; volume v; price p;};
const price MARKET_NOT_EXIST = -1;
class OpenBooks;
class Order {
public:
    explicit Order(ordtype t = ordtype::limit, 
                   side s    = side::buy,
                   volume v  = 0L, 
                   price  p  = 0L) noexcept:
        _o(_id++), _t(t), _s(s), _v(v), _p(p){}
    inline ordid   getoid() const {return _o;}
    inline volume  getqty() const {return _v;}
    inline price   getpri() const {return _p;}
    inline ordtype gettyp() const {return _t;}
    inline void    setqty(volume v) {_v =v;}
    virtual bool   matchable(OpenBooks&){return false;}
    virtual void   transform(){}
    friend std::ostream& operator << (std::ostream& os, const Order& o);
private:
    static int   _id;
    const   ordid   _o;             // order number 
protected:
    mutable ordtype _t;
    const   side    _s; 
    mutable volume  _v;
    mutable price   _p;
    side getoppo() const; 
};
int Order::_id = 1;


class OpenBooks {
public:
    OpenBooks();
    bool   recvOrder(ordtype, side, volume, price);
    void   process();
    price  getMarketPrice(side)const;
    std::vector<match_record>           _matches;
private:
    cmpfunc _cmp_b;
    mutable std::priority_queue<cmpfct, std::vector<cmpfct>, 
        decltype(_cmp_b)> _limit_order_b;
    cmpfunc _cmp_s;
    mutable std::priority_queue<cmpfct, std::vector<cmpfct>, 
        decltype(_cmp_s)> _limit_order_s;

    side _s;  // current order process is buy side or sell side
    std::vector<Order>                  _orders;
    std::map<price, std::vector<ordid>> _stop_order  [2];
    std::set<ordid>                     _cancel_order;
    std::vector<ordid>                  _market_order[2];
    Order *                             _pOrder;

    Order* _createOrder(ordtype, side, volume, price);
    void   _execute(Order*, ordtype);
    void   _deposit(Order*, ordtype);
};
OpenBooks::OpenBooks():_cmp_b([](cmpfct l, cmpfct r)
        {return l.p<r.p ||(l.p==r.p && l.o>r.o);}),_limit_order_b(_cmp_b),
                       _cmp_s([](cmpfct l, cmpfct r)
        {return l.p>r.p ||(l.p==r.p && l.o>r.o);}),_limit_order_s(_cmp_s),
                       _pOrder(nullptr)
{
    _orders.reserve(64);
    _matches.reserve(64);
    for(auto v:_market_order) v.reserve(64);
}
side Order::getoppo() const {
    if (_s==side::none) 
        return _s;
    else 
        return _s==side::buy?side::sell:side::buy;
}

class LimitOrder:public Order{
public:
    LimitOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual bool matchable(OpenBooks&);
};

class MarketOrder:public Order{
public:
    MarketOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual bool matchable(OpenBooks&);
};

class CancelOrder:public Order{
public:
    CancelOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual bool matchable(OpenBooks&);
};

class StopOrder:public Order{
public:
    StopOrder(ordtype o, side s, volume v, price p):
        Order(o, s, v, p){}
    virtual bool matchable(OpenBooks&);
    virtual void transform(){
        _t = ordtype::market;
        _p = 0;
    }
};

bool LimitOrder::matchable(OpenBooks& ob) {
    price p = ob.getMarketPrice(getoppo());
    return _v > 0 &&    // volume should > 0 
           p !=  MARKET_NOT_EXIST &&  
          ((_s==side::sell && _p<=p)|| 
           (_s==side::buy  && _p>=p));
}

bool MarketOrder::matchable(OpenBooks& ob) {
    price p = ob.getMarketPrice(getoppo());
    return _v > 0 &&    // volume should > 0 
           p != MARKET_NOT_EXIST;
}

bool StopOrder::matchable(OpenBooks& ob) {
    price p = ob.getMarketPrice(getoppo());
    return _v > 0 &&
           p !=  MARKET_NOT_EXIST &&  
           ((_s==side::sell && _p>=p)||
            (_s==side::buy  && _p<=p));
}

bool CancelOrder::matchable(OpenBooks& ob) {
    price p = ob.getMarketPrice(getoppo());
    return _v > 0 &&
           p !=  MARKET_NOT_EXIST &&  
           ((_s==side::sell && _p>=p)||
            (_s==side::buy  && _p<=p));
}

bool OpenBooks::recvOrder(ordtype t, side s, volume v, price p) {
    _pOrder = _createOrder(t, s, v, p);
    if (_pOrder==nullptr) return false;
    _orders.push_back(*_pOrder);  // deposit 
    _s = s;                       // buy side or sell side
    return true;
}

void OpenBooks::process() {
    ordtype t = _pOrder->gettyp(); 
    switch(t) {
    case ordtype::limit:
    case ordtype::market:
        while (_pOrder->matchable(*this)) // continuous matching
            _execute(_pOrder, t); 
        if (_pOrder->getqty()) { // not execute on its entirety
            _deposit(_pOrder, t);
        }
        break;
    case ordtype::stop:
        if (_pOrder->matchable(*this)) { 
            _pOrder->transform();  // stop transform to market order
            this->process();
        } 
        else {
            _deposit(_pOrder, t);
        }
        break;
    case ordtype::cancel:
        //if (_pOrder->matchable(*this))  
        _execute(_pOrder, t);
        break;
    }
}

void OpenBooks::_deposit(Order* pOrder, ordtype t) {
    pqueue *pq[2] = {&_limit_order_b, &_limit_order_s}; 
    int is;
    switch (_s){
        case side::buy:  is = 0;break;
        case side::sell: is = 1;break;
        case side::none: is =-1;break;
    }
    switch(t) {
    case ordtype::limit:
    {
        ordid o = _pOrder->getoid();
        price p = _pOrder->getpri();
        volume v = _pOrder->getqty();
        std::cout<<o<<" "<<p<<" "<<v<<std::endl;
        pq[is]->push({_pOrder->getpri(), 
                      _pOrder->getqty(), 
                      _pOrder->getoid()});
    }
        break;
    case ordtype::stop:
    {
        price p = _pOrder->getpri();
        ordid o = _pOrder->getoid();
        auto got = _stop_order[is].find(p);
        if (got!=_stop_order[is].end()) 
            got->second.push_back(o);
        else 
            _stop_order[is].insert({p, {o}});
    }
    break;
    // the unmatched part of the market order will be dropped 
    case ordtype::market: break; 
    case ordtype::cancel: break;
    }
}
void OpenBooks::_execute(Order* pOrder, ordtype t) {
    pqueue *pq[2] = {&_limit_order_s, &_limit_order_b}; 
    int is;
    switch (_s){
        case side::buy:  is = 0;break;
        case side::sell: is = 1;break;
        case side::none: is =-1;break;
    }
    switch(t) {
        case ordtype::limit:
        case ordtype::market:
        {
            ordid  matched_o = pq[is]->top().o;
            price  matched_p = pq[is]->top().p;
            volume matched_v = pq[is]->top().v; 
            volume v = pOrder->getqty();
            if (matched_v> v){
                _matches.push_back({pOrder->getoid(), matched_o, 
                                    v, matched_p}); 
                pOrder->setqty(0);
                pq[is]->pop();
                pq[is]->push({matched_p, matched_v-v, matched_o});
            }
            else {
                _matches.push_back({pOrder->getoid(), matched_o, 
                                    matched_v, matched_p}); 
                pOrder->setqty(v-matched_v);
                pq[is]->pop();
            }
        }
        break;
        case ordtype::cancel:
        { 
            ordid o = _pOrder->getqty(); //for cancel order, this is 
                                         // the order will be canceled
            ordid o_top_s = -1, o_top_b = -1; 
            if (!pq[0]->empty()) o_top_s = pq[0]->top().o;
            if (!pq[1]->empty()) o_top_b = pq[1]->top().o;
            if (_pOrder->getoid()<o) {
                std::cerr << "error cancel unexist order"<<std::endl;
                return;
            }
            // the cancel order happens to be on the top
            else if (o_top_s!=-1 && o_top_s == o) pq[0]->pop();
            else if (o_top_b!=-1 && o_top_b == o) pq[1]->pop();
            else _cancel_order.insert(o);
        }
        break;
        case ordtype::stop: break;
    }
}
price OpenBooks::getMarketPrice(side s)const {
    pqueue *pq = nullptr; 
    if (s==side::buy) pq = &_limit_order_b;
    else pq = &_limit_order_s;

    // make sure the fetched market price belong to 
    // an order not been canceled
    while( !pq->empty()) {
        ordid o = pq->top().o;
        if (_cancel_order.find(o)==_cancel_order.end()) 
            return pq->top().p;
        pq->pop();
    }
    return MARKET_NOT_EXIST;
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
        default: return (char*)"na";
    }
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
    exch.process();
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
    for(auto v : exch._matches) 
        std::cout<<"match " << v.taker << " " <<v.maker << " " 
                 <<v.v << " " 
                 << (long)(v.p/100) << "." 
                 << std::setfill('0') << std::setw(2) 
                 << (int)(v.p%100)
                 <<std::endl;;
    return 0;
}
