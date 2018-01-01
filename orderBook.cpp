#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <queue>

enum class Side {
    buy = 0,
    sell = 1
};
using ordid = int;
using price = int;  // the price contains 2 fixed decimal, so we 
                    // multiply all price with 100 to avoid float computing 
struct compare_factor {price p;ordid o;};  // price, ordid
using cmpfct = struct compare_factor;

struct OpenBooks 
{
public:
    OpenBooks();
    std::vector<order>                _orders;

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
    int buy = static_cast<int>(Side:buy);
    int sell= static_cast<int>(Side:sell);
    _cmp[buy] = [](cmpfct l, cmpfct r){
        return l.p<r.p || l.p==r.p && l.o>r.o;};
    _cmp[sell] = [](cmpfct l, cmpfct r){ 
        return l.p>r.p || l.p==r.p && l.o>r.o;};
}

bool parseLine(const std::string & line, OpenBooks & ob)
{
    char ordtyp[8] = {0x0};
    char side[8] = {0x0};
    long vol = 0L;
    float price = 0.;
    
    const int narg = std::sscanf(line.c_str(), 
             "%6s %4s %ld %f", ordtyp, side, &vol, &price);
    return true;
    if (narg != 4) 
    {
        return false;
    }
    const Order ord(ordtyp[0], side[0], vol, price); 
    process(ob, ord);
    return true;
}

int main(int argc, char * argv[])
{
    OpenBooks ob;
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (!parseLine(line, ob))
        {
            std::cerr << "Error while parsing line \"" << line << "\"" << std::endl;
            return 1;
        }
    }
    return 0;
}
