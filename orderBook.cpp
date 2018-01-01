#include <iostream>
#include <string>
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

class Order
{
public:
    explicit Order(ordtype t = ordtype::limit, 
                   side s    = side::buy,
                   volume v  = 0L, 
                   price  p  = 0L) noexcept:
        _o(_id), _id(_id++), _t(t), _s(s), _v(v), _p(p){}
    friend ostream& operator << (ostream& os, const Order& o);
private:
    static int _id;
    const ordid _o = 1;             // order number 
    const ordtype _t;
    const side _s; 
    const volume _v;
    const price _p;
    inline char* const _side2str() const  noexcept;
    inline char* const _ordtype2str() const noexcept ;
};

struct OpenBooks 
{
public:
    OpenBooks();
    std::vector<Order>                _orders;

    std::function<bool(const cmpfct l, const cmpfct r)> _cmp[2];

    int buy = static_cast<int>(Side:buy); 
    int sell= static_cast<int>(Side:sell);

    std::priority_queue<cmpfct, std::vector<cmpfct>, 
        decltype(_cmp[buy])>  _limit_order(_cmp[buy]);
    std::priority_queue<cmpfct, std::vector<cmpfct>, 
        decltype(_cmp[sell])> _limit_order(_cmp[sell]);

    std::map<price, ordid>            _stop_order  [2];
    std::set<ordid>                   _cancel_order[2];
    std::vector<ordid>                _market_order[2];
};

OpenBooks::OpenBooks() {
    _orders.reserve(64);
    int buy = static_cast<int>(Side:buy);
    int sell= static_cast<int>(Side:sell);
    _cmp[buy] = [](cmpfct l, cmpfct r){
        return l.p<r.p || l.p==r.p && l.o>r.o;};
    _cmp[sell] = [](cmpfct l, cmpfct r){ 
        return l.p>r.p || l.p==r.p && l.o>r.o;};
}


inline char* const Order::_side2str() const noexcept {
    switch (_s){
        case side::buy:  return "buy";
        case side::sell: return "sell";
    }
    return "na";
}

inline char* const Order::_ordtype2str() const noexcept {
    switch (_t){
        case ordtype::limit: return "limit";
        case ordtype::market:return "market";
        case ordtype::stop:  return "stop";
        case ordtype::cancel:return "cancel";
    }
    return "na";
}

ostream& operator<<(ostream& os, const Order& o) {
    os<<o._ordtype2str()<<" "<<o._side2str()<<" "
      <<o._v<<" "<<(long)(o._p/100) << "."
      <<(int)(o._p%100);
    return os;
}

void process(OpenBooks& exch, Order& od) {
    exch._orders.push_back(od);
}

bool parseLine(const std::string & line, OpenBooks & exch)
{
    char ordtyp[8] = {0x0};
    char side[8]   = {0x0};
    long vol       = 0L;
    long iprice    = 0L; // integer part of price
    int  dprice    = 0L; // decimal part of price
    
    int narg = std::sscanf(line.c_str(),"%6s %4s %ld %ld.%d", 
             ordtyp, side, &vol, &iprice, &dprice);
    if (narg != 5) {
        return false;
    }
    Order od(static_cast<ordtype>(ordtyp[0]), 
             static_cast<side>(side[0]), 
             vol, 
             iprice*100+dprice); 
    process(exch, od);
    return true;
}

int main(int argc, char * argv[])
{
    OpenBooks exch;
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (!parseLine(line, exch))
        {
            std::cerr << "Error while parsing line \"" 
                      << line << "\"" << std::endl;
            return 1;
        }
    }
    // print
    for(auto v : exch._orders) {
        std::cout<<v<<std::endl;
    } 
    return 0;
}
