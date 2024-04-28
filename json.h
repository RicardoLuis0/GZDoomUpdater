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

#include <variant>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <stdexcept>
#include <optional>


#include "util.h"

enum JSON_Literal {
    JSON_FALSE,
    JSON_TRUE,
    JSON_NULL,
};

namespace JSON {
    class Element;
    using object_t=std::map<std::string,Element>;
    using array_t=std::vector<Element>;
    
    inline std::string json_except_format(const std::string &pre,const std::string &expected,const std::string &is){
        return pre+"Expected type "+Util::quote_str_single(expected)+", got type "+Util::quote_str_single(is);
    }
    inline std::string json_except_format(const std::string &pre,const std::vector<std::string> &expected,const std::string &is){
        return pre+"Expected "+(expected.size()==1?"type":"types")+" "+Util::join_or(Util::map(expected,Util::quote_str_single))+", got type "+Util::quote_str_single(is);
    }
    inline std::string json_except_format(const std::string &expected,const std::string &is){
        return json_except_format("",expected,is);
    }
    inline std::string json_except_format(const std::vector<std::string> &expected,const std::string &is){
        return json_except_format("",expected,is);
    }
    
    class JSON_Exception : public std::runtime_error {
    public:
        std::string msg_top;
        JSON_Exception(const std::string &s):runtime_error("JSON: "+s),msg_top(s){}
        
        JSON_Exception(const std::string &pre,const std::string &expected,const std::string &is):
            JSON_Exception(json_except_format(pre,expected,is)){}
        JSON_Exception(const std::string &pre,const std::vector<std::string> &expected,const std::string &is):
            JSON_Exception(json_except_format(pre,expected,is)){}
        JSON_Exception(const std::string &expected,const std::string &is):
            JSON_Exception(json_except_format(expected,is)){}
        JSON_Exception(const std::vector<std::string> &expected,const std::string &is):
            JSON_Exception(json_except_format(expected,is)){}
    };
    
    class Element {
        public:
            
            using data_t = std::variant<int64_t,double,std::string,array_t,object_t,JSON_Literal>;
            data_t data;
            
            //explicit constructors using std::variant
            explicit inline Element(const data_t & v) : data(v) {}
            explicit inline Element(data_t && v) : data(std::move(v)) {}
            
            //conversion constructors for simple data types
            inline Element(int i) : data(i) {}
            inline Element(int64_t i) : data(i) {}
            inline Element(double d) : data(d) {}
            inline Element(const std::string &s) : data(s) {}
            inline Element(std::string &&s) : data(std::move(s)) {}
            inline Element(bool b) : data(b?JSON_TRUE:JSON_FALSE) {}
            inline Element(std::nullptr_t) : data(JSON_NULL) {}
            inline Element(JSON_Literal l) : data(l) {}
            
            //helper access methods, may throw std::bad_variant_access if trying to access wrong types
            inline int64_t& get_int(){ return is_int()?std::get<int64_t>(data):throw JSON_Exception("Integer",type_name()); }
            inline const int64_t& get_int() const { return is_int()?std::get<int64_t>(data):throw JSON_Exception("Integer",type_name()); }
            
            inline double& get_double(){ return is_double()?std::get<double>(data):throw JSON_Exception("Double",type_name()); }
            inline const double& get_double() const { return is_double()?std::get<double>(data):throw JSON_Exception("Double",type_name()); }
            
            inline int64_t get_number_int() const { return is_double()?static_cast<int64_t>(std::get<double>(data)):is_int()?std::get<int64_t>(data):throw JSON_Exception("Number",type_name()); }
            inline double get_number_double() const { return is_double()?std::get<double>(data):is_int()?static_cast<double>(std::get<int64_t>(data)):throw JSON_Exception("Number",type_name()); }
            
            inline std::string& get_str(){ return is_str()?std::get<std::string>(data):throw JSON_Exception("String",type_name()); }
            inline const std::string& get_str() const { return is_str()?std::get<std::string>(data):throw JSON_Exception("String",type_name()); }
            
            inline array_t& get_arr(){ return is_arr()?std::get<array_t>(data):throw JSON_Exception("Array",type_name()); }
            inline const array_t& get_arr() const { return is_arr()?std::get<array_t>(data):throw JSON_Exception("Array",type_name()); }
            
            inline object_t& get_obj(){ return is_obj()?std::get<object_t>(data):throw JSON_Exception("Object",type_name()); }
            inline const object_t& get_obj() const { return is_obj()?std::get<object_t>(data):throw JSON_Exception("Object",type_name()); }
            
            inline bool get_bool() const { return std::holds_alternative<JSON_Literal>(data)?(std::get<JSON_Literal>(data)!=JSON_NULL?std::get<JSON_Literal>(data)==JSON_TRUE:throw JSON_Exception("Boolean",type_name())):throw JSON_Exception("Boolean",type_name()); }
            
            //helper type check methods
            
            inline bool is_int() const { return std::holds_alternative<int64_t>(data); }
            
            inline bool is_double() const { return std::holds_alternative<double>(data); }
            
            inline bool is_number() const { return std::holds_alternative<int64_t>(data)||std::holds_alternative<double>(data); }
            
            inline bool is_str() const { return std::holds_alternative<std::string>(data); }
            
            inline bool is_arr() const { return std::holds_alternative<std::vector<Element>>(data); }
            
            inline bool is_obj() const { return std::holds_alternative<std::map<std::string,Element>>(data); }
            
            inline bool is_bool() const { return std::holds_alternative<JSON_Literal>(data)&&std::get<JSON_Literal>(data)!=JSON_NULL; }
            
            inline bool is_null() const { return std::holds_alternative<JSON_Literal>(data)&&std::get<JSON_Literal>(data)==JSON_NULL; }
            
            inline const char * type_name() const {
                if(is_int()){
                    return "Integer";
                }else if(is_double()){
                    return "Double";
                }else if(is_str()){
                    return "String";
                }else if(is_arr()){
                    return "Array";
                }else if(is_obj()){
                    return "Object";
                }else if(is_null()){
                    return "Null";
                }else if(is_bool()){
                    return "Boolean";
                }else{
                    return "Unknown";
                }
            }
            
            //serialize with spaces/newlines
            std::string to_json(bool trailing_quote=true,size_t depth=0) const;
            
            //serialize without spaces/newlines
            std::string to_json_min() const;
            
            Element& operator[](size_t index){//array access
                return get_arr().at(index);
            }
            
            Element& operator[](const char * index){//object access
                return get_obj().at(index);
            }
            
            Element& operator[](std::string index){//object access
                return get_obj().at(index);
            }
            
            operator int64_t(){
                return is_int()?get_int():is_double()?get_double():throw std::bad_variant_access();
            }
            
            operator double(){
                return is_int()?get_int():is_double()?get_double():throw std::bad_variant_access();
            }
            
            operator std::string(){
                return is_str()?get_str():throw std::bad_variant_access();
            }
            
    };
    
    inline Element Int(int64_t i){ return Element(i); }
    inline Element Boolean(bool b){ return Element(b?JSON_TRUE:JSON_FALSE); }
    inline Element True(){ return Element(JSON_TRUE); }
    inline Element False(){ return Element(JSON_FALSE); }
    inline Element Null(){ return Element(JSON_NULL); }
    inline Element Double(double d){ return Element(d); }
    inline Element String(std::string s){ return Element(s); }
    inline Element Array(const std::vector<Element> & v){ return Element(Element::data_t(v)); }
    inline Element Array(std::vector<Element> && v){ return Element(Element::data_t(std::move(v))); }
    inline Element Object(const std::map<std::string,Element> & m){ return Element(Element::data_t(m)); }
    inline Element Object(std::map<std::string,Element> && m){ return Element(Element::data_t(std::move(m))); }
    
    Element parse(const std::string &data);
    
}
