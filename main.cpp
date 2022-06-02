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

#include <cstdint>
#include <cstdarg>
#include <atomic>
#include <thread>
#include <filesystem>


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <process.h>
#include <CommCtrl.h>

#include <zip.h>

#include "resource.h"

#include "json.h"

#include <curl/curl.h>

#define GZDOOM_FILENAME L"gzdoom.exe"

namespace std {
    namespace fs=filesystem;
}

struct VersionTriplet {
    int major;
    int minor;
    int patch;
};

static HANDLE hProcessHeap;

//get version from existing gzdoom.exe
static VersionTriplet getCurrentVersion(){
    int size = GetFileVersionInfoSizeW(
                    GZDOOM_FILENAME,    // lptstrFilename
                    NULL                // ignored
               );
    
    if(!size) {
        if(PathFileExistsW(GZDOOM_FILENAME)){ // if gzdoom.exe exists, GetFileVersionInfoSizeW failed
            MessageBox(NULL,L"getCurrentVersion - GetFileVersionInfoSizeW failed",NULL,MB_OK|MB_ICONERROR);
            exit(EXIT_FAILURE);
        } else { // gzdoom.exe is missing
            MessageBox(NULL,L"Cannot find gzdoom.exe -- is the program in the correct folder?",NULL,MB_OK|MB_ICONERROR);
            exit(EXIT_FAILURE);
        }
    }
    
    void * buffer = HeapAlloc(hProcessHeap,HEAP_ZERO_MEMORY,size);
    
    if(!buffer) { // HeapAlloc failed
        MessageBox(NULL,L"getCurrentVersion - HeapAlloc failed",NULL,MB_OK|MB_ICONERROR);
        exit(EXIT_FAILURE);
    }
    
    int version_info_ok = GetFileVersionInfoW(
            GZDOOM_FILENAME,    // lptstrFilename
            0,                  // ignored
            size,               // dwLen
            buffer              // lpData
         );
    
    if(!version_info_ok) { // GetFileVersionInfoW failed
        MessageBox(NULL,L"getCurrentVersion - GetFileVersionInfoW failed",NULL,MB_OK|MB_ICONERROR);
        HeapFree(hProcessHeap,0,buffer);
        exit(EXIT_FAILURE);
    }
    
    VS_FIXEDFILEINFO * info;
    UINT info_size;
    
    int info_ok=VerQueryValueW(
                    buffer,         // pBlock
                    L"\\",          // lpSubBlock
                    (void**)&info,  // lplpBuffer
                    &info_size      // puLen
                );
    
    if(!info_ok) { // VerQueryValueW "\" failed
        MessageBox(NULL,L"getCurrentVersion - VerQueryValueW \"\\\" failed",NULL,MB_OK|MB_ICONERROR);
        HeapFree(hProcessHeap,0,buffer);
        exit(EXIT_FAILURE);
    } 
    
    if(info_size!=sizeof(VS_FIXEDFILEINFO)) { // VerQueryValueW "\" returned wrong size
        MessageBox(NULL,L"getCurrentVersion - VerQueryValueW \"\\\" returned wrong size for VS_FIXEDFILEINFO",NULL,MB_OK|MB_ICONERROR);
        HeapFree(hProcessHeap,0,buffer);
        exit(EXIT_FAILURE);
    }
    
    VersionTriplet version {HIWORD(info->dwFileVersionMS),LOWORD(info->dwFileVersionMS),HIWORD(info->dwFileVersionLS)};
    HeapFree(hProcessHeap,0,buffer);
    return version;
}

static size_t curl_write_text(void *buffer, size_t size, size_t nmemb, void *userp){
    if(userp){
        std::string * out=static_cast<std::string*>(userp);
        (*out)+=std::string(static_cast<char*>(buffer),size*nmemb);//ugly hack, but should be _fine_ as long as the data is text
    }
    return nmemb;
}

static JSON::Element latest_release_data(JSON_NULL);

static VersionTriplet getLatestVersion(){
    if(curl_global_init(CURL_GLOBAL_WIN32|CURL_GLOBAL_SSL)){
        MessageBox(NULL,L"curl_global_init failed",NULL,MB_OK|MB_ICONERROR);
        exit(EXIT_FAILURE);
    }
    std::string version_json_str;
    CURL * curl=curl_easy_init();
    if(curl){
        curl_easy_setopt(curl,CURLOPT_URL,"https://api.github.com/repos/coelckers/gzdoom/releases/latest");
        
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,curl_write_text);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&version_json_str);
        curl_easy_setopt(curl,CURLOPT_HEADERDATA,nullptr);
        
        curl_easy_setopt(curl,CURLOPT_USERAGENT,"GZDoom Updater");
        curl_easy_setopt(curl,CURLOPT_ACCEPT_ENCODING,"application/vnd.github.v3+json");
        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
        
        curl_easy_setopt(curl,CURLOPT_SSL_OPTIONS,CURLSSLOPT_NATIVE_CA);
        
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,1L);
        
        int err = curl_easy_perform(curl);
        if(err != CURLE_OK){
            //curl_easy_perform failed -- no internet?
            return (VersionTriplet){0,0,0};
        }
        
        curl_easy_cleanup(curl);
        
        try{
            latest_release_data=JSON::parse(version_json_str);
        }catch(JSON::JSON_Exception &e){
            //JSON parse failed
            MessageBoxW(NULL,L"Json Parse Failed",NULL,MB_OK|MB_ICONERROR);
            MessageBoxA(NULL,e.what(),NULL,MB_OK|MB_ICONERROR);
            return (VersionTriplet){0,0,0};
        }
        
        std::string version_str = latest_release_data.get_obj().at("tag_name");
        
        if(version_str.size()>2&&version_str[0]=='g'&&version_str[1]>='0'&&version_str[1]<='9'){
            std::vector<std::string> version_triplet_str=Util::split(version_str.substr(1),'.',true);
            return (VersionTriplet){stoi(version_triplet_str[0]),((version_triplet_str.size()>1)?(stoi(version_triplet_str[1])):0),((version_triplet_str.size()>2)?(stoi(version_triplet_str[2])):0)};
        }else{
            return (VersionTriplet){0,0,0};
        }
    }else{
        MessageBox(NULL,L"curl_easy_init failed",NULL,MB_OK|MB_ICONERROR);
        exit(EXIT_FAILURE);
    }
}

enum download_sig {
    DL_B,
    DL_KB,
    DL_MB,
    DL_GB,
};

static const char * download_sig_str[] {
    "B",
    "KB",
    "MB",
    "GB",
};

static volatile std::atomic_bool aborted=false;
static volatile std::atomic_bool finished=false;

//test data
/*
static std::atomic_int download_cur=20;
static std::atomic_int download_cur_sig=DL_MB;
static std::atomic_int download_max=200;
static std::atomic_int download_max_sig=DL_MB;
static std::atomic_int download_percent_10000=1000;
*/

static volatile int download_cur=0;
static volatile int download_cur_sig=0;
static volatile int download_max=0;
static volatile int download_max_sig=0;
static volatile int download_percent_10000=0;

static INT_PTR CALLBACK DialogProc(HWND hDialog,UINT msg,WPARAM wParam,LPARAM lParam){
    switch(msg){
    case WM_INITDIALOG:{
            SetTimer(hDialog,0,100, NULL);
            
            //center window
            
            RECT desktop_rect;
            GetWindowRect(GetDesktopWindow(),&desktop_rect);
            
            RECT dialog_rect;
            GetWindowRect(hDialog, &dialog_rect);
            
            LONG dialog_width=dialog_rect.right-dialog_rect.left;
            LONG dialog_height=dialog_rect.bottom-dialog_rect.top;
            
            LONG desktop_width=desktop_rect.right-desktop_rect.left;
            LONG desktop_height=desktop_rect.bottom-desktop_rect.top;
            
            SetWindowPos(
                hDialog,                            // hWnd
                NULL,                               // hWndInsertAfter
                (desktop_width-dialog_width)/2,     // X
                (desktop_height-dialog_height)/2,   // Y
                0,                                  // cx
                0,                                  // cy
                SWP_NOSIZE|SWP_NOZORDER             // uFlags
            );
            SendMessage(GetDlgItem(hDialog,IDC_PROGRESS1),PBM_SETRANGE,0,MAKELPARAM(0,10000));
        }
        //fallthrough
    case WM_TIMER:
        if(finished){
            EndDialog(hDialog,0);
        }else{
            int download_whole=download_percent_10000/100;
            int download_frac=download_percent_10000%100;
            std::wstring label_new_text(Util::str_wprintf(L"%d%s / %d%s (%d.%02d%%)",(int)download_cur,download_sig_str[download_cur_sig],(int)download_max,download_sig_str[download_max_sig],download_whole,download_frac));
            SetWindowTextW(GetDlgItem(hDialog,IDC_LABEL1),label_new_text.c_str());
            SendMessage(GetDlgItem(hDialog,IDC_PROGRESS1),PBM_SETPOS,download_percent_10000,0);
        }
        break;
    case WM_COMMAND:
    case WM_CLOSE:
        aborted=true;
        EndDialog(hDialog,0);
        break;
    default:
        return false;
    }
    return true;
}

static void openProgressDialog(HINSTANCE hInst){
    DialogBoxW(hInst,MAKEINTRESOURCEW(IDD_DIALOG1),NULL,DialogProc);
}

static int updateProgressBar(void * clientp,curl_off_t dltotal,curl_off_t dlnow,curl_off_t ultotal,curl_off_t ulnow) {
    if(dltotal>=int64_t(1_G)) {
        download_max=dltotal/1_G;
        download_max_sig=DL_GB;
    } else if(dltotal>=int64_t(1_M)) {
        download_max=dltotal/1_M;
        download_max_sig=DL_MB;
    } else if(dltotal>=int64_t(1_K)) {
        download_max=dltotal/1_K;
        download_max_sig=DL_KB;
    } else {
        download_max=dltotal/1_K;
        download_max_sig=DL_B;
    }
    
    if(dlnow>=int64_t(1_G)) {
        download_cur=dlnow/1_G;
        download_cur_sig=DL_GB;
    } else if(dlnow>=int64_t(1_M)) {
        download_cur=dlnow/1_M;
        download_cur_sig=DL_MB;
    } else if(dlnow>=int64_t(1_K)) {
        download_cur=dlnow/1_K;
        download_cur_sig=DL_KB;
    } else {
        download_cur=dlnow/1_K;
        download_cur_sig=DL_B;
    }
    
    if(dltotal>0){
        download_percent_10000=dlnow/(dltotal/10000);
    }
    return aborted;
}

static size_t curl_write_binary(void *buffer, size_t size, size_t nmemb, void *userp){
    if(userp&&!aborted){
        try{
            std::vector<std::byte> * data=static_cast<std::vector<std::byte> *>(userp);
            size_t sz=data->size();
            size_t len=size*nmemb;
            data->resize(sz+len);
            memcpy(static_cast<void*>(data->data()+sz),buffer,len);
        }catch(...){
            aborted=true;
        }
    }
    return nmemb;
}


static std::string gzdoom_download_url;
static std::vector<std::byte> gzdoom_bin;

static void downloaderThreadProc(){
    CURL * curl=curl_easy_init();
    if(curl){
        char * url=gzdoom_download_url.data();
        curl_easy_setopt(curl,CURLOPT_URL,url);
        
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,curl_write_binary);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&gzdoom_bin);
        curl_easy_setopt(curl,CURLOPT_HEADERDATA,nullptr);
        curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,&updateProgressBar);
        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
        
        
        curl_easy_setopt(curl,CURLOPT_ACCEPT_ENCODING,"");
        
        curl_easy_setopt(curl,CURLOPT_USERAGENT,"GZDoom Updater");
        
        curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
        
        curl_easy_setopt(curl,CURLOPT_MAXREDIRS,50L);
        
        curl_easy_setopt(curl,CURLOPT_SSL_OPTIONS,CURLSSLOPT_NATIVE_CA);
        
        int err=curl_easy_perform(curl);
        
        if(err!=CURLE_OK){
            //curl_easy_perform failed
            aborted=true;
        }else{
            long code=0;
            curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
            if(code!=200){
                aborted=true;
            }
        }
        curl_easy_cleanup(curl);
    } else {
        aborted=true;
    }
    finished=!aborted;
}

static std::thread downloaderThread;

static void startDownload(){
    downloaderThread=std::thread(downloaderThreadProc);
}

static std::string GetErrorStr(DWORD dwErr){
    LPSTR lpBuffer;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwErr,0,(LPSTR)&lpBuffer,1,NULL);
    std::string out(lpBuffer);
    LocalFree(lpBuffer);
    return out;
}


static bool tryDeleteAll(std::vector<std::fs::path> files){
    std::vector<HANDLE> file_handles;
    file_handles.reserve(files.size());
    std::fs::path err_file;
    std::string err;
    bool ok=true;
    for(auto &file : files){
        HANDLE h=CreateFileA(file.string().c_str(),DELETE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if(h==INVALID_HANDLE_VALUE){
            err_file=file;
            DWORD dwErr=GetLastError();
            if(dwErr==ERROR_FILE_NOT_FOUND||dwErr==ERROR_PATH_NOT_FOUND){
                continue;//inexistent files are ok
            }
            err=GetErrorStr(dwErr);
            ok=false;
            break;
        }
        FILE_DISPOSITION_INFO fdi;
        fdi.DeleteFile=1;
        BOOL bResult=SetFileInformationByHandle(h,FileDispositionInfo,&fdi,sizeof(FILE_DISPOSITION_INFO));
        if(!bResult){
            CloseHandle(h);
            err_file=file;
            err=GetErrorStr(GetLastError());
            ok=false;
            break;
        }
        file_handles.push_back(h);
    }
    if(!ok){
        for(auto &hFile:file_handles){
            FILE_DISPOSITION_INFO fdi;
            fdi.DeleteFile=0;
            SetFileInformationByHandle(hFile,FileDispositionInfo,&fdi,sizeof(FILE_DISPOSITION_INFO));
        }
    }
    for(auto &hFile:file_handles){
        CloseHandle(hFile);
    }
    if(!ok){
        MessageBoxA(NULL,Util::str_printf("Aborting Update\nFailed to Delete '%s': %s",err_file.string().c_str(),err.c_str()).c_str(),NULL,MB_OK|MB_ICONERROR);
    }
    return ok;
}

bool fatal_unzip_error=false;//unzipping failure left the installation unworkable?

bool zip_file_is_directory(std::fs::path file_path){
    return (file_path.string().back()=='/');
}

static void unzipGZDoom(){
    
    zip_error_t err;
    zip_error_init(&err);
    
    zip_source_t * data=zip_source_buffer_create(gzdoom_bin.data(),gzdoom_bin.size(),0,&err);
    if(!data){
        MessageBoxA(NULL,Util::str_printf("Failed to Open Zip: %s",zip_error_strerror(&err)).c_str(),NULL,MB_OK|MB_ICONERROR);
        return;
    }
    
    zip_t * archive=zip_open_from_source(data,ZIP_CHECKCONS|ZIP_RDONLY,&err);
    if(!archive){
        zip_source_free(data);
        MessageBoxA(NULL,Util::str_printf("Failed to Open Zip: %s",zip_error_strerror(&err)).c_str(),NULL,MB_OK|MB_ICONERROR);
        return;
    }
    
    bool files_extracted=false;
    bool files_deleted=false;
    bool files_created=false;
    
    std::vector<std::fs::path> files_to_delete;
    std::vector<zip_stat_t> files_to_extract;
    
    struct uncompressed_file_t {
        
        uncompressed_file_t(std::vector<std::byte> && _file_data,const char * _file_path):file_data(std::move(_file_data)),file_path(_file_path){
        }
        
        std::vector<std::byte> file_data;
        std::fs::path file_path;
    };
    
    std::vector<uncompressed_file_t> uncompressed_file_data;
    
    try{
        size_t n=zip_get_num_entries(archive,0);
        
        for(size_t i=0;i<n;i++){
            zip_stat_t info;
            zip_stat_index(archive,i,0,&info);
            if((info.valid&ZIP_STAT_NAME)&&(info.valid&ZIP_STAT_INDEX)&&(info.valid&ZIP_STAT_SIZE)){//check if info has needed fields
                std::fs::path p(info.name);
                if(!zip_file_is_directory(p)){
                    files_to_extract.emplace_back(info);
                    if(std::fs::exists(p)&&!std::fs::is_directory(p)){
                        files_to_delete.emplace_back(std::move(p));
                    }
                }
            }
        }
        
        files_extracted=true;
        for(const zip_stat_t &info:files_to_extract){
            zip_file_t * zf=zip_fopen_index(archive,info.index,0);
            if(zf){
                try{
                    std::vector<std::byte> data;
                    data.resize(info.size);
                    zip_int64_t result=zip_fread(zf,data.data(),info.size);
                    files_extracted=((result>0)&&(((size_t)result)==info.size));
                    zip_fclose(zf);
                    if(files_extracted){
                        uncompressed_file_data.emplace_back(std::move(data),info.name);
                    }else{
                        break;
                    }
                }catch(...){
                    zip_fclose(zf);
                    throw;
                }
            }else{
                files_extracted=false;
                break;
            }
        }
        zip_close(archive);
    }catch(...){
        zip_close(archive);
        throw;
    }
    try{
        if(files_extracted){
            files_deleted=tryDeleteAll(files_to_delete);
            if(files_deleted){
                for(auto &data:uncompressed_file_data){
                    std::error_code e;
                    std::fs::create_directories(data.file_path.parent_path(),e);
                    Util::writefile_binary(data.file_path.string(),data.file_data);
                }
                files_created=true;
            }
        }else{
            MessageBox(NULL,L"Failed to Extract Files",NULL,MB_OK|MB_ICONERROR);
        }
    }catch(...){
        fatal_unzip_error=files_deleted&&!files_created;
        throw;
    }
    return;
}

static void updateGZDoom(HINSTANCE hInst){
    //find url for win64
    {
        const JSON::array_t &assets=latest_release_data.get_obj().at("assets").get_arr();
        bool found=false;
        for(const JSON::Element &asset_e:assets){
            const JSON::object_t &asset=asset_e.get_obj();
            const std::string &name=asset.at("name").get_str();
            if((name.find("Windows")!=std::string::npos)&&(name.find("64bit")!=std::string::npos)){
                found=true;
                gzdoom_download_url=asset.at("browser_download_url").get_str();
                break;
            }
        }
        if(!found){
            return;
        }
    }
    
    startDownload();
    
    openProgressDialog(hInst);
    
    downloaderThread.join();
    
    if(aborted){
        return;
    }
    
    if(!finished){
        MessageBox(NULL,L"Invalid Program State",NULL,MB_OK|MB_ICONERROR);
        exit(EXIT_FAILURE);
    }
    try{
        unzipGZDoom();
        if(fatal_unzip_error){
            MessageBox(NULL,L"Fatal Error while Unzipping -- You may need to manually reinstall GZDoom",NULL,MB_OK|MB_ICONERROR);
        }
    }catch(...){
        if(fatal_unzip_error){
            MessageBox(NULL,L"Fatal Error while Unzipping -- You may need to manually reinstall GZDoom",NULL,MB_OK|MB_ICONERROR);
        }
        throw;
    }
}

[[noreturn]] static void runGZDoom(PCWSTR *argv){
    _wexecv(GZDOOM_FILENAME,argv);
    MessageBox(NULL,L"Couldn't run gzdoom.exe",NULL,MB_OK|MB_ICONERROR);
    exit(EXIT_FAILURE);
}

int wWinMain(
    HINSTANCE hInst,
    HINSTANCE hInstPrev,
    PWSTR     pCmdLine,
    int       nCmdShow
) try {
    hProcessHeap = GetProcessHeap();
    
    if(!hProcessHeap) { // GetProcessHeap failed
        MessageBox(NULL,L"GZDoomUpdater - GetProcessHeap failed",NULL,MB_OK|MB_ICONERROR);
        exit(EXIT_FAILURE);
    }
    
    VersionTriplet current_version=getCurrentVersion();
    
    VersionTriplet latest_version=getLatestVersion();
    
    if(current_version.major<latest_version.major||(current_version.major==latest_version.major&&current_version.minor<latest_version.minor)||(current_version.major==latest_version.major&&current_version.minor==latest_version.minor&&current_version.patch<latest_version.patch)){
        updateGZDoom(hInst);
    }
    
    curl_global_cleanup();
    if(!fatal_unzip_error){ 
        int argc;
        runGZDoom((PCWSTR *)CommandLineToArgvW(GetCommandLineW(),&argc));
    }else{
        exit(EXIT_FAILURE);
    }
} catch (std::exception &e){
    MessageBoxA(NULL,e.what(),"Unhandled Exception",MB_OK|MB_ICONERROR);
    exit(EXIT_FAILURE);
    
}
