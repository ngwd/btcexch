#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <functional>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <algorithm>
#include <memory>

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

inline int const side2int(side s) {
    switch (s){
        case side::buy:  return 0; 
        case side::sell: return 1; 
        default: return -1;
    }
};

using ordid  = int;
using price  = long;  
using volume = long;

struct compare_factor {price p;volume v;ordid o;};  // price, quantity ordid
using cmpfct = struct compare_factor;
using cmpfunc = std::function<bool(const cmpfct l, const cmpfct r)>;
using pqueue = std::priority_queue<cmpfct, std::vector<cmpfct>, cmpfunc> ;

class Order;
class OpenBooks;
using spo = std::shared_ptr<Order> ;

struct match_record   {ordid taker; ordid maker; volume v; price p;};
const price MARKET_NOT_EXIST = -1;
const ordid NO_ORDER = -1;

class Order {
public:
    explicit Order(ordid   o,
                   ordtype t = ordtype::limit, 
                   side s    = side::buy,
                   volume v  = 0L, 
                   price  p  = 0L) noexcept:
        _o(o), _t(t), _s(s), _v(v), _p(p){}

    explicit Order(ordtype t = ordtype::limit, 
                   side s    = side::buy,
                   volume v  = 0L, 
                   price  p  = 0L) noexcept:
        _o(_id++), _t(t), _s(s), _v(v), _p(p){}
    inline ordid   getoid() const {return _o;}
    inline volume  getqty() const {return _v;}
    inline price   getpri() const {return _p;}
    inline ordtype gettyp() const {return _t;}
    inline side    getsid() const {return _s;}
    inline void    setsid(side  s) {_s = s;}
    inline void    setqty(volume v){_v = v;}
    virtual bool   matchable(OpenBooks&){return false;}
    virtual void   transform(){}
    friend std::ostream& operator << (std::ostream& os, const Order& o);
private:
    static int   _id;
    const ordid   _o;             // order number 
protected:
    mutable ordtype _t;
    mutable side    _s; 
    mutable volume  _v;
    mutable price   _p;
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
    spo                                 _pOrder;
    std::vector<Order>                  _orders;
    std::set<ordid>                     _cancel_order;

    std::map<price, std::vector<ordid>> _stop_order  [2];
    std::vector<ordid>                  _market_order[2];

    spo  _createOrder(ordtype, side, volume, price);
    void   _execute(spo, ordtype);
    void   _deposit(spo, ordtype);
    void   _notify(price, price, side); 
};
OpenBooks::OpenBooks():_cmp_b(
[](cmpfct l, cmpfct r){return l.p<r.p ||(l.p==r.p && l.o>r.o);}),
_limit_order_b(_cmp_b), _cmp_s(
[](cmpfct l, cmpfct r){return l.p>r.p ||(l.p==r.p && l.o>r.o);}),
_limit_order_s(_cmp_s), _pOrder(nullptr) {
    _orders.reserve(64);
    _matches.reserve(64);
    for(auto v:_market_order) v.reserve(64);
}

inline const side getoppo(side s) {
    if (s==side::none) 
        return s;
    else 
        return s==side::buy?side::sell:side::buy;
}

class LimitOrder:public Order{
public:
    LimitOrder(ordtype t, side s, volume v, price p):
        Order(t, s, v, p){}
    virtual bool matchable(OpenBooks&);
};

class MarketOrder:public Order{
public:
    MarketOrder(ordid o, ordtype t, side s, volume v, price p):
        Order(o, t, s, v, p){}
    MarketOrder(ordtype t, side s, volume v, price p):
        Order(t, s, v, p){}
    virtual bool matchable(OpenBooks&);
};

class CancelOrder:public Order{
public:
    CancelOrder(ordtype t, side s, volume v, price p):
        Order(t, s, v, p){}
};

class StopOrder:public Order{
public:
    StopOrder(ordtype t, side s, volume v, price p):
        Order(t, s, v, p){}
    virtual bool matchable(OpenBooks&);
    virtual void transform(){
        _t = ordtype::market;
        _p = 0;
    }
};

bool LimitOrder::matchable(OpenBooks& ob) {
    price p = ob.getMarketPrice(getoppo(_s));
    return _v > 0 &&    // volume should > 0 
           p !=  MARKET_NOT_EXIST &&  
          ((_s==side::sell && _p<=p)|| 
           (_s==side::buy  && _p>=p));
}

bool MarketOrder::matchable(OpenBooks& ob) {
    price p = ob.getMarketPrice(getoppo(_s));
    return _v > 0 &&    // volume should > 0 
           p != MARKET_NOT_EXIST;
}

bool StopOrder::matchable(OpenBooks& ob) {
    price p = ob.getMarketPrice(getoppo(_s));
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
    side oppo_s = getoppo(_s);
    int is      = side2int(_s);
    price p_pre = getMarketPrice(oppo_s); 

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
        _execute(_pOrder, t);
        break;
    }
    price p_now = getMarketPrice(oppo_s); 
    // top of heap change, market price change need to notify _stop_order;
    _notify(p_pre, p_now, _s);

    // process those stop orders transfering to market order
    while (!_market_order[is].empty()) {
        ordid o = _market_order[is].back();
        _market_order[is].pop_back();
        _s = _orders[o-1].getsid();
        _pOrder = spo(new MarketOrder(o, ordtype::market,  
                                _s, _orders[o-1].getqty(),
                                0));
        this->process();
    }
}

void OpenBooks::_deposit(spo pOrder, ordtype t) {
    pqueue *pq[2] = {&_limit_order_b, &_limit_order_s}; 
    int is = side2int(_s);
    switch(t) {
    case ordtype::limit:
    {
        ordid o = pOrder->getoid();
        price p = pOrder->getpri();
        volume v = pOrder->getqty();
        pq[is]->push({pOrder->getpri(), 
                      pOrder->getqty(), 
                      pOrder->getoid()});
    }
    break;
    case ordtype::stop:
    {
        price p = pOrder->getpri();
        ordid o = pOrder->getoid();
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
void OpenBooks::_execute(spo pOrder, ordtype t) {
    pqueue *pq[2] = {&_limit_order_s, &_limit_order_b}; 
    int is = side2int(_s);
    switch(t) {
        case ordtype::limit:
        case ordtype::market:
        {
            // top means the top of the heap(priority queue)
            ordid  top_o = pq[is]->top().o;
            price  top_p = pq[is]->top().p;
            volume top_v = pq[is]->top().v; 
            volume v = pOrder->getqty();
            if (top_v>v){
                _matches.push_back({pOrder->getoid(), top_o, 
                                    v, top_p}); // log the match result
                pOrder->setqty(0);
                pq[is]->pop();
                pq[is]->push({top_p, top_v-v, top_o});
            }
            else {
                _matches.push_back({pOrder->getoid(), top_o, 
                                 top_v, top_p});// log the match result 
                pOrder->setqty(v-top_v);
                pq[is]->pop();
            }
        }
        break;
        case ordtype::cancel:
        { 
            ordid o = pOrder->getqty(); //for cancel order, this is 
                                         // the order will be canceled
            ordid o_top_s = NO_ORDER, o_top_b = NO_ORDER; 
            if (!pq[0]->empty()) o_top_s = pq[0]->top().o;
            if (!pq[1]->empty()) o_top_b = pq[1]->top().o;
            if (pOrder->getoid()<o) {
                std::cerr << "error cancel unexist order"<<std::endl;
                return;
            }
            // the cancel order happens to be on the top
            else if (o_top_s!=NO_ORDER && o_top_s == o) {
                pq[0]->pop();
                // top of heap change, market price change
                // need to send the price to _stop_book[buy];
            }
            else if (o_top_b!=NO_ORDER && o_top_b == o) {
                pq[1]->pop();
                // top of heap change, market price change
                // need to send the price to _stop_book[sell];
            }
            else _cancel_order.insert(o);
        }
        break;
        case ordtype::stop: break;
    }
}
void OpenBooks::_notify(price pre, price now, side s) {
    if (pre == now || now ==MARKET_NOT_EXIST) return;
    int is = side2int(s);

    std::map<price, std::vector<ordid>>::const_iterator start, end;
    if (s == side::sell) {
        start = _stop_order[is].lower_bound(now); 
        end = _stop_order[is].end();
    }
    else {  // buy side 
        start = _stop_order[is].begin(); 
        end = _stop_order[is].upper_bound(now);  
    }
    for (auto it = start; it!=end; ++it) {
        for (auto v: it->second){
            _market_order[is].push_back(v);
        }
    }
    _stop_order[is].erase(start, end);
    std::sort(_market_order[is].begin(), 
              _market_order[is].end(), 
              std::greater<ordid>());    
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

spo OpenBooks::_createOrder(ordtype t, side s, volume v, price p) {
    switch (t) {
        case ordtype::limit :return spo (new LimitOrder (t, s, v, p));
        case ordtype::stop  :return spo (new StopOrder  (t, s, v, p));
        case ordtype::cancel:return spo (new CancelOrder(t, s, v, p));
        case ordtype::market:return spo (new MarketOrder(t, s, v, p));
    }
    return nullptr; 
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

int main(int argc, char * argv[]) {
    OpenBooks exch;
    std::ifstream inf("odbk_test1");
    char line[100];
    //std::string line;
    // while (std::getline(std::cin, line))
    while(!inf.getline(line, 100).eof()) {
        if (!parseLine(line, exch)) {
            std::cerr << "Error while parsing line \"" 
                      << line << "\"" << std::endl;
            return 1;
        }
    }
    // print the final matches
    for(auto v : exch._matches) 
        std::cout<<"match " << v.taker << " " <<v.maker << " " 
                 <<v.v << " " 
                 << (long)(v.p/100) << "." 
                 << std::setfill('0') << std::setw(2) 
                 << (int)(v.p%100)
                 <<std::endl;;
    return 0;
}
