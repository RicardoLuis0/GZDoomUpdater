/**
  * Permission is hereby granted, free of charge, to any person obtaining a copy of this
  * software and associated documentation files (the "Software"), to deal in the Software
  * without restriction, including without limitation the rights to use, copy, modify,
  * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
  * permit persons to whom the Software is furnished to do so.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  */

#pragma once

#include <map>
#include <vector>
#include <algorithm>
#include <functional>
#include <string>
#include <limits>

inline std::string operator"" _s(const char * s,size_t n){
    return {s,n};
}

constexpr unsigned long long int operator"" _K(unsigned long long int n){
    return n*1024;
}

constexpr unsigned long long int operator"" _M(unsigned long long int n){
    return n*1024_K;
}

constexpr unsigned long long int operator"" _G(unsigned long long int n){
    return n*1024_M;
}

namespace Util {
    
    std::string str_printf(const char * fmt,...) __attribute__((format(printf,1,2)));
    std::wstring str_wprintf(const wchar_t * fmt,...) /* __attribute__((format(wprintf,1,2))) */ ;
    
    std::string readfile(const std::string &filename);
    void writefile(const std::string &filename,const std::string &data);
    void writefile_binary(const std::string &filename,const std::vector<std::byte> &data);
    
    std::string quote_str(const std::string &s,char quote_char);
    
    inline std::string quote_str_double(const std::string &s){
        return quote_str(s,'"');
    }
    
    inline std::string quote_str_single(const std::string &s){
        return quote_str(s,'\'');
    }
    
    template<typename K,typename V>
    std::vector<K> keys(const std::map<K,V> &m){
        std::vector<K> k;
        k.reserve(m.size());
        for(const auto &kv:m){
            k.push_back(kv.first);
        }
        return k;
    }
    
    template<typename K,typename V>
    std::vector<V> values(const std::map<K,V> &m){
        std::vector<V> v;
        v.reserve(m.size());
        for(const auto &kv:m){
            v.push_back(kv.second);
        }
        return v;
    }
    
    std::string join(const std::vector<std::string> &v,const std::string &on=" ");
    
    std::string join_or(const std::vector<std::string> &v,const std::string &sep_comma=", ",const std::string &sep_or=", or ");
    
    std::vector<std::string> split(const std::string &str,char split_on,bool split_empty=false);
    std::vector<std::string> split(const std::string &str,const std::vector<char> &split_on,bool split_empty=false);
    std::vector<std::string> split_str(const std::string &str,const std::string &split_on,bool split_empty=false);
    std::vector<std::string> split_str(const std::string &str,const std::vector<std::string> &split_on,bool split_empty=false);
    
    template<typename R,typename Fn_T>
    std::vector<typename std::invoke_result<Fn_T,typename R::value_type>::type> map(const R &r,const Fn_T &f) {
        std::vector<typename std::invoke_result<Fn_T,typename R::value_type>::type> v;
        v.reserve(std::size(r));
        for(const auto &e:r){
            v.emplace_back(std::invoke(f,e));
        }
        return v;
    }
    
    template<typename R,typename Fn_T>
    std::vector<typename std::invoke_result<Fn_T,typename R::value_type>::type> map_copy(const R &r,const Fn_T &f) {
        std::vector<typename std::invoke_result<Fn_T,typename R::value_type>::type> v;
        v.reserve(std::size(r));
        for(const auto &e:r){
            v.push_back(std::invoke(f,e));
        }
        return v;
    }
    
    template<typename T,typename R>
    std::vector<T> map_construct(const R &r) {
        std::vector<T> v;
        v.reserve(std::size(r));
        for(const auto &e:r){
            v.emplace_back(e);
        }
        return v;
    }
    
    template<typename R,typename Fn_T>
    R& inplace_map(R &r,const Fn_T &f) {
        for(auto &e:r){
            e=std::invoke(f,e);
        }
        return r;
    }
    
    inline std::vector<const char *> get_cstrs(const std::vector<std::string> &v){
        return Util::map(v,&std::string::c_str);
    }
    
    template<typename Ra,typename ... Rb>
    std::vector<typename Ra::value_type> merge(const Ra &ra,const Rb & ... rb) {
        std::vector<typename Ra::value_type> o(std::begin(ra),std::end(ra));
        o.reserve(std::size(ra)+(std::size(rb)+...));
        (std::copy(std::begin(rb),std::end(rb),std::back_inserter(o)),...);
        return o;
    }
    
    inline std::string str_tolower(std::string s){
        return inplace_map(s,&tolower);
    }
    
    template<typename T,size_t N>
    struct CArrayIteratorAdaptor {
        using value_type=T;
        using size_type=size_t;
        using difference_type=std::ptrdiff_t;
        
        using reference=T&;
        using pointer=T*;
        using iterator=T*;
        using reverse_iterator=std::reverse_iterator<T*>;
        
        using const_reference=const T&;
        using const_pointer=const T*;
        using const_iterator=const T*;
        using const_reverse_iterator=std::reverse_iterator<const T*>;
        T * const _data;
        
        CArrayIteratorAdaptor(T (&a)[N]) : _data(a) {
        }
        
        static constexpr size_type size() noexcept {
            return N;
        }
        
        static constexpr size_type max_size() noexcept {
            return (std::numeric_limits<size_type>::max()/sizeof(value_type))-1;
        }
        
        static constexpr size_type empty() noexcept {
            return N==0;
        }
        
        constexpr const_iterator begin() const noexcept {
            return _data;
        }
        
        constexpr const_iterator end() const noexcept {
            return _data+N;
        }
        
        constexpr iterator begin() noexcept {
            return _data;
        }
        
        constexpr iterator end() noexcept {
            return _data+N;
        }
        
        constexpr const_iterator cbegin() const noexcept {
            return _data;
        }
        
        constexpr const_iterator cend() const noexcept {
            return _data+N;
        }
        
        constexpr reverse_iterator rbegin() noexcept {
            return std::make_reverse_iterator(end());
        }
        
        constexpr reverse_iterator rend() noexcept {
            return std::make_reverse_iterator(begin());
        }
        
        
        constexpr const_reverse_iterator rbegin() const noexcept {
            return std::make_reverse_iterator(end());
        }
        
        constexpr const_reverse_iterator rend() const noexcept {
            return std::make_reverse_iterator(begin());
        }
        
        constexpr const_reverse_iterator crbegin() const noexcept {
            return std::make_reverse_iterator(end());
        }
        
        constexpr const_reverse_iterator crend() const noexcept {
            return std::make_reverse_iterator(begin());
        }
        
        constexpr const_pointer data() const noexcept {
            return _data;
        }
        
        constexpr pointer data() noexcept {
            return _data;
        }
        
        constexpr reference front() noexcept {
            return _data[0];
        }
        
        constexpr reference back() noexcept {
            return _data[N-1];
        }
        
        constexpr reference operator[](size_t i) noexcept {
            return _data[i];
        }
        
        constexpr const_reference front() const noexcept {
            return _data[0];
        }
        
        constexpr const_reference back() const noexcept {
            return _data[N-1];
        }
        
        constexpr const_reference operator[](size_t i) const noexcept {
            return _data[i];
        }
        
    };
    
    template<size_t N,typename T>
    constexpr size_t arr_len(T (&)[N]){
        return N;
    }
}

