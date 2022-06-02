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

#include "json.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace JSON {
    
    namespace {
        
        constexpr bool is_whitespace(char c){
            return c==' '||c=='\t'||c=='\r'||c=='\n';
        }
        
        constexpr bool is_word_start(char c){
            return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';
        }
        
        constexpr bool is_number(char c){
            return (c>='0'&&c<='9');
        }
        
        constexpr bool is_word_char(char c){
            return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';
        }
        
        bool is_number_start_nosign(const std::string &data, size_t i){
            return is_number(data[i])||(data[i]=='.'&&(i+1<data.size())&&is_number(data[i+1]));
        }
        
        bool is_number_start(const std::string &data, size_t i){
            return is_number_start_nosign(data,i)||((data[i]=='-'||data[i]=='+')&&(i+1<data.size())&&is_number_start_nosign(data,i+1));
        }
        
        Element get_number(const std::string &data, size_t &i){//handles integers, decimals and scientific notation
            if(i>=data.size()) throw std::runtime_error("Expected Number, got EOF");
            union {
                int64_t i=0;
                double d;
            } num;
            bool is_double=false;
            bool valid=false;
            bool is_negative=data[i]=='-';
            ssize_t double_depth=1;
            
            if(data[i]=='-'||data[i]=='+')i++;
            
            for(;i<data.size();i++){
                if(!is_double&&data[i]=='.'){
                    is_double=true;
                    num.d=num.i;
                }else if(data[i]=='e'||data[i]=='E'){
                    i++;
                    bool negative=false;
                    ssize_t exponent=0;
                    valid=false;
                    if(i<data.size()&&(data[i]=='+'||data[i]=='-')){
                        if(data[i]=='-')negative=true;
                        i++;
                    }
                    for(;i<data.size();i++){
                        if(data[i]>='0'&&data[i]<='9'){
                            valid=true;
                            (exponent*=10)+=data[i]-'0';
                        }else{
                            break;
                        }
                    }
                    if(is_double){
                        num.d=num.d*std::pow(10,negative?-exponent:exponent);
                    }else{
                        num.d=num.i*std::pow(10,negative?-exponent:exponent);
                        is_double=true;
                    }
                    break;
                }else if(data[i]>='0'&&data[i]<='9'){
                    valid=true;
                    if(is_double){
                        num.d+=(data[i]-'0')*std::pow(10,-(double_depth++));
                    }else{
                        (num.i*=10)+=data[i]-'0';
                    }
                }else{
                    break;
                }
            }
            
            if(!valid){
                if(data.size()>=i){
                    throw std::runtime_error("Expected Number, got EOF");
                }else{
                    throw std::runtime_error(std::string("Expected Number, got '")+data[i]+"' at pos "+std::to_string(i));
                }
            }else if(is_double){
                return is_negative?-num.d:num.d;
            }else{
                return is_negative?-num.i:num.i;
            }
        }
        
        constexpr char unescape(char c){
            switch(c) {
            case 'a':
                return '\a';
            case 'b':
                return '\b';
            case 'e':
                return '\e';
            case 'f':
                return '\f';
            case 'n':
                return '\n';
            case 'r':
                return '\r';
            case 't':
                return '\t';
            case 'v':
                return '\v';
            case '\\':
                return '\\';
            case '"':
                return '\"';
            default:
                return c;
            }
        }
        
        std::string escape_char_str(char c){
            switch(c) {
            case '\a':
                return "\\a";
            case '\b':
                return "\\b";
            case '\e':
                return "\\e";
            case '\f':
                return "\\f";
            case '\n':
                return "\\n";
            case '\r':
                return "\\r";
            case '\t':
                return "\\t";
            case '\v':
                return "\\v";
            case '\\':
                return "\\\\";
            case '"':
                return "\\\"";
            default:
                return std::string(1,c);
            }
        }
        
        inline bool is_char(const std::string &data, size_t &i,char c){
            return (i<data.size())&&(data[i]==c);
        }
        
        inline void expect_char(const std::string &data, size_t &i,char c){
            if(i>=data.size()) throw std::runtime_error("Expected '"+escape_char_str(c)+"', got EOF");
            if(data[i]!=c) throw std::runtime_error("Expected '"+escape_char_str(c)+"', got '"+data[i]+"' at pos "+std::to_string(i));
        }
        
        Element get_string(const std::string &data, size_t &i){
            expect_char(data,i,'"');
            i++;
            size_t start=i;
            size_t n=0;
            for(;i<data.size();i++){
                if(data[i]=='\n'){
                    continue;
                }else if(data[i]=='\\'){
                    i++;
                    n++;
                }else if(data[i]=='"'){
                    std::string str;
                    str.reserve(n);
                    for(size_t j=start;j<i;j++){
                        if(data[j]=='\n'){
                            continue;
                        }else if(data[j]=='\\'){
                            j++;
                            str+=unescape(data[j]);
                        }else{
                            str+=data[j];
                        }
                    }
                    i++;
                    return str;
                }else{
                    n++;
                }
            }
            throw std::runtime_error("Expected '\"', got EOF");
        }
        
        void skip_whitespace(const std::string &data, size_t &i){ //SAFE TO CALL ON EOF, TODO skip comments
            while(i<data.size()){
                if(is_whitespace(data[i])){
                    i++;
                }else if(data[i]=='#'){
                    i++;
                    while(i<data.size()&&data[i]!='\n')i++;
                    if(i<data.size())i++;
                }else if(data[i]=='/'&&(i+1<data.size())&&(data[i+1]=='/'||data[i+1]=='*')){
                    if(data[i+1]=='/'){
                        i+=2;
                        while(i<data.size()&&data[i]!='\n')i++;
                        if(i<data.size())i++;
                    }else{
                        i+=2;
                        if(i<data.size())i++;
                        while(i<data.size()&&!(data[i-1]=='*'&&data[i]=='/'))i++;
                        if(i<data.size())i++;
                    }
                }else{
                    break;
                }
            }
        }
        
        Element get_element(const std::string &data, size_t &i);
        
        Element get_array(const std::string &data, size_t &i){
            expect_char(data,i,'[');
            i++;
            skip_whitespace(data,i);
            if(is_char(data,i,']')){
                i++;
                return JSON::Array({});
            }
            std::vector<Element> v;
            while(i<data.size()){
                skip_whitespace(data,i);
                v.emplace_back(get_element(data,i));
                skip_whitespace(data,i);
                if(i<data.size()&&data[i]==']'){
                    i++;
                    return JSON::Array(std::move(v));
                }
                expect_char(data,i,',');
                i++;
                skip_whitespace(data,i);
                if(i<data.size()&&data[i]==']'){
                    i++;
                    return JSON::Array(std::move(v));
                }
            }
            throw std::runtime_error("Expected ']', got EOF");
        }
        
        Element get_object(const std::string &data, size_t &i){
            expect_char(data,i,'{');
            i++;
            skip_whitespace(data,i);
            if(is_char(data,i,'}')){
                i++;
                return JSON::Object({});
            }
            std::map<std::string,Element> m;
            while(i<data.size()){
                skip_whitespace(data,i);
                std::string key=std::move(std::get<std::string>(get_string(data,i).data));
                skip_whitespace(data,i);
                expect_char(data,i,':');
                i++;
                skip_whitespace(data,i);
                m.insert({key,get_element(data,i)});
                skip_whitespace(data,i);
                if(i<data.size()&&data[i]=='}'){
                    i++;
                    return JSON::Object(std::move(m));
                }
                expect_char(data,i,',');
                i++;
                skip_whitespace(data,i);
                if(i<data.size()&&data[i]=='}'){
                    i++;
                    return JSON::Object(std::move(m));
                }
            }
            throw std::runtime_error("Expected '}', got EOF");
        }
        
        Element get_element(const std::string &data, size_t &i){
            skip_whitespace(data,i);
            if(i>=data.size()) throw std::runtime_error("Expected JSON, got EOF");
            switch(data[i]){
            case '[':
                return get_array(data,i);
            case '{':
                return get_object(data,i);
            case '"':
                return get_string(data,i);
            default:
                if(is_number_start(data,i)){
                    return get_number(data,i);
                }else if((i+3)<data.size()&&data[i]=='n'&&data[i+1]=='u'&&data[i+2]=='l'&&data[i+3]=='l'){
                    i+=4;
                    return JSON_NULL;
                }else if((i+3)<data.size()&&data[i]=='t'&&data[i+1]=='r'&&data[i+2]=='u'&&data[i+3]=='e'){
                    i+=4;
                    return JSON_TRUE;
                }else if((i+4)<data.size()&&data[i]=='f'&&data[i+1]=='a'&&data[i+2]=='l'&&data[i+3]=='s'&&data[i+4]=='e'){
                    i+=5;
                    return JSON_FALSE;
                }
            }
            throw std::runtime_error(std::string("Expected JSON, got '")+data[i]+"' at pos "+std::to_string(i));
        }
        
        constexpr char escape(char c){
            switch(c) {
            case '\a':
                return 'a';
            case '\b':
                return 'b';
            case '\e':
                return 'e';
            case '\f':
                return 'f';
            case '\n':
                return 'n';
            case '\r':
                return 'r';
            case '\t':
                return 't';
            case '\v':
                return 'v';
            case '\\':
                return '\\';
            case '"':
                return '"';
            default:
                return c;
            }
        }
        
        inline std::string quote_str(std::string s){
            std::string str;
            str.reserve(s.size()+10);
            str+='"';
            for(char c:s){
                if(c=='\\'||c=='"'||escape(c)!=c){
                    str+='\\';
                    str+=escape(c);
                }else{
                    str+=c;
                }
            }
            str+='"';
            return str;
        }
        
        inline std::string indent(size_t depth){
            return std::string(depth*4,' ');
        }
    }
    
    std::string Element::to_json(bool trailing_quote,size_t depth) const {
        if(std::holds_alternative<int64_t>(data)){//int
            return std::to_string(get_int());
        }else if(std::holds_alternative<double>(data)){//double
            return std::to_string(get_double());
        }else if(std::holds_alternative<std::string>(data)){//string
            return quote_str(get_str());
        }else if(std::holds_alternative<JSON_Literal>(data)){//literal
            return std::get<JSON_Literal>(data)==JSON_TRUE?"true":std::get<JSON_Literal>(data)==JSON_FALSE?"false":"null";
        }else if(std::holds_alternative<std::vector<Element>>(data)){//array
            return "[\n"+({
                std::string s;
                bool first=true;
                for(auto &e:get_arr()){
                    if(!first)s+=",\n";
                    s+=indent(depth+1);
                    s+=e.to_json(trailing_quote,depth+1);
                    first=false;
                }
                if(!s.empty())s+=trailing_quote?",\n":"\n";
                s;
            })+indent(depth)+"]";
        }else if(std::holds_alternative<std::map<std::string,Element>>(data)){//object
            return "{\n"+({
                std::string s;
                bool first=true;
                for(auto &e:get_obj()){
                    if(!first)s+=",\n";
                    s+=indent(depth+1);
                    s+=quote_str(e.first);
                    s+=" : ";
                    s+=e.second.to_json(trailing_quote,depth+1);
                    first=false;
                }
                if(!s.empty())s+=trailing_quote?",\n":"\n";
                s;
            })+indent(depth)+"}";
        }
        __builtin_unreachable();//all std::variant cases are handled in the if/else, this is absolutely unreachable
    }
    
    std::string Element::to_json_min() const {
        if(std::holds_alternative<int64_t>(data)){//int
            return std::to_string(get_int());
        }else if(std::holds_alternative<double>(data)){//double
            return std::to_string(get_double());
        }else if(std::holds_alternative<std::string>(data)){//string
            return quote_str(get_str());
        }else if(std::holds_alternative<JSON_Literal>(data)){//literal
            return std::get<JSON_Literal>(data)==JSON_TRUE?"true":std::get<JSON_Literal>(data)==JSON_FALSE?"false":"null";
        }else if(std::holds_alternative<std::vector<Element>>(data)){//array
            return "["+({
                std::string s;
                bool first=true;
                for(auto &e:get_arr()){
                    if(!first)s+=",";
                    s+=e.to_json_min();
                    first=false;
                }
                s;
            })+"]";
        }else if(std::holds_alternative<std::map<std::string,Element>>(data)){//object
            return "{"+({
                std::string s;
                bool first=true;
                for(auto &e:get_obj()){
                    if(!first)s+=",";
                    s+=quote_str(e.first);
                    s+=":";
                    s+=e.second.to_json_min();
                    first=false;
                }
                s;
            })+"}";
        }
        __builtin_unreachable();//all std::variant cases are handled in the if/else, this is absolutely unreachable
    }
    
    Element parse(const std::string &data){
        size_t i=0;
        return get_element(data,i);
    }
    
}
