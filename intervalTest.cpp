void intervaltest () {
    /*
     market price (limit buy) change will affect the stop sell order collection, 
     some stop sell order need to turned to market sell order
     return a vector<ordid>, denote the new market sell order id descendently
    */
    enum{buy=0,sell};
    map<int, list<int>> _stop_books;
    _stop_books[9948] = {3, 4};
    _stop_books[9952] = {1};    
    _stop_books[9944] = {7};
    _stop_books[9943] = {8, 10};
    _stop_books[9942] = {9};
    vector<int> _mkt_books; _mkt_books.reserve(16);
    
    auto bnd = _stop_books.lower_bound(9944);    
    for (auto it = bnd;it!=_stop_books.end(); ++it) {
        // cout<<it->first<<": ";
        for (auto v: it->second){
            _mkt_books.push_back(v);
            // cout<<v <<", ";
        }
        // cout<<endl;
    }
    
    std::sort(_mkt_books.begin(), _mkt_books.end(), std::greater<int>());    
    _stop_books.erase(bnd, _stop_books.end());
    // print
    /*
    for (auto v: _mkt_books) {
        cout<<v<<", ";
    }
    cout<<endl;
    for (auto u : _stop_books) {
        cout<<u.first<<": ";
        for (auto v: u.second){            
            cout<<v <<", ";
        }
        cout<<endl;
    }
    cout<<endl;
    */
}

void intervaltest1 () {
    /*
     market price (limit sell) change will affect the stop buy order collection,
     some stop buy order need to turned to market buy order
     return a vector<ordid>, denote the new market buy order id descendently
    */
    enum{buy=0,sell};
    map<int, list<int>> _stop_books;
    _stop_books[9948] = {3, 4};
    _stop_books[9952] = {1};    
    _stop_books[9944] = {7};
    _stop_books[9943] = {8, 10};
    _stop_books[9942] = {9};
    
    auto bnd = _stop_books.upper_bound(9944);
    vector<int> _mkt_books; _mkt_books.reserve(16);
    for (auto it = _stop_books.begin();it!=bnd; ++it) {
        cout<<it->first<<": ";
        for (auto v: it->second){
            _mkt_books.push_back(v);
            cout<<v <<", ";
        }
        cout<<endl;
    }
    
    std::sort(_mkt_books.begin(), _mkt_books.end(), std::greater<int>());    
    _stop_books.erase(_stop_books.begin(), bnd);
    // print
    /*
    for (auto v: _mkt_books) {
        cout<<v<<", ";
    }
    cout<<endl;
    for (auto u : _stop_books) {
        cout<<u.first<<": ";
        for (auto v: u.second){            
            cout<<v <<", ";
        }
        cout<<endl;
    }
    cout<<endl;
    */
}
