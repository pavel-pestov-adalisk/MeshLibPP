#include "MRSystem.h"
#include "MRStringConvert.h"
#include "MRConfig.h"
#include "MRStacktrace.h"
#include "MRPch/MRSpdlog.h"
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include "psapi.h"

#ifndef _MSC_VER
#include <cpuid.h>
#endif
#ifdef __MINGW32__
#include <windows.h>
#endif

#else //not Windows

#if defined(__APPLE__)
#include <sys/sysctl.h>
#ifndef MRMESH_NO_CLIPBOARD
#include <clip/clip.h>
#endif
#else
#include "MRPch/MRWasm.h"
#ifndef __EMSCRIPTEN__
  #ifndef __ARM_CPU__
    #include <cpuid.h>
  #endif
  #ifndef MRMESH_NO_CLIPBOARD
    #include <clip/clip.h>
  #endif
#endif
#endif
#include <pthread.h>
#include <libgen.h>
#include <unistd.h>
#include <limits.h>
#include <regex>
#include <pwd.h>

#endif

#if defined(__APPLE__) && defined(__MACH__)
     #include <mach-o/dyld.h>
#endif

#ifndef MR_PROJECT_NAME
#define MR_PROJECT_NAME "MeshInspector"
#endif

namespace MR
{

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
// If true, the resources should be loaded from the executable directory, rather than from the system directories.
[[nodiscard]] static bool resourcesAreNearExe()
{
    auto opt = std::getenv("MR_LOCAL_RESOURCES");
    return opt && std::string_view(opt) == "1";
}
#endif


void SetCurrentThreadName( const char * name )
{
#ifdef _MSC_VER
    const DWORD MS_VC_EXCEPTION = 0x406D1388;

    #pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
    #pragma pack(pop)

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags = 0;

    #pragma warning(push)
    #pragma warning(disable: 6320 6322)
    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    #pragma warning(pop)
#elif defined(__APPLE__) && defined(__MACH__)
    pthread_setname_np(name);
#elif defined(__EMSCRIPTEN__)
    (void)name;
#else
    pthread_setname_np( pthread_self(), name);
#endif
}


std::filesystem::path GetExeDirectory()
{
#ifdef _WIN32
    HMODULE hm = GetModuleHandleA( "MRMesh.dll" );
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW( hm, szPath, MAX_PATH );
#else
    #ifdef __EMSCRIPTEN__
        return "/";
    #endif
    char szPath[PATH_MAX];
    #ifdef __APPLE__
          uint32_t size = PATH_MAX + 1;

          if (_NSGetExecutablePath(szPath, &size) != 0) {
            // Buffer size is too small.
            spdlog::error( "Executable directory is too long" );
            return {};
          }
          szPath[size] = '\0';
    #else
        ssize_t count = readlink( "/proc/self/exe", szPath, PATH_MAX );
        if( count < 0 )
        {
            spdlog::error( "Executable directory was not found" );
            return {};
        }
        if( count >= PATH_MAX )
        {
            spdlog::error( "Executable directory is too long" );
            return {};
        }
        szPath[count] = '\0';
    #endif
#endif
    auto res = std::filesystem::path{ szPath }.parent_path() / "";
    return res;
}

std::filesystem::path GetResourcesDirectory()
{
    auto exePath = GetExeDirectory();
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
    return exePath;
#else
    if ( resourcesAreNearExe() )
        return exePath;
    #ifdef __APPLE__
        #ifdef MR_FRAMEWORK
    return "/Library/Frameworks/" + std::string( MR_PROJECT_NAME ) + ".framework/Versions/Current/Resources/";
        #else
    return "/Applications/" + std::string( MR_PROJECT_NAME ) + ".app/Contents/Resources/";
        #endif
    #else
    return "/usr/local/etc/" + std::string( MR_PROJECT_NAME ) + "/";
    #endif
#endif
}

std::filesystem::path GetFontsDirectory()
{
    auto exePath = GetExeDirectory();
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
    return exePath;
#else
    if ( resourcesAreNearExe() )
        return exePath;
    #ifdef __APPLE__
    return GetResourcesDirectory() / "fonts/";
    #else
    return "/usr/local/share/fonts/";
    #endif
#endif
}

std::filesystem::path GetLibsDirectory()
{
    auto exePath = GetExeDirectory();
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
    return exePath;
#else
    if ( resourcesAreNearExe() )
        return exePath;
    #ifdef __APPLE__
        #ifdef MR_FRAMEWORK
    return "/Library/Frameworks/" + std::string( MR_PROJECT_NAME ) + ".framework/Versions/Current/lib/";
        #else
    return "/Applications/" + std::string( MR_PROJECT_NAME ) + ".app/Contents/libs/";
        #endif
    #else
    return "/usr/local/lib/" + std::string( MR_PROJECT_NAME ) + "/";
    #endif
#endif
}

std::filesystem::path GetEmbeddedPythonDirectory()
{
    auto exePath = GetExeDirectory();
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
    return exePath;
#else
    if ( resourcesAreNearExe() )
        return exePath;
#ifdef __APPLE__
#ifdef MR_FRAMEWORK
    return "/Library/Frameworks/" + std::string( MR_PROJECT_NAME ) + ".framework/Versions/Current/Frameworks/";
#else
    return "/Applications/" + std::string( MR_PROJECT_NAME ) + ".app/Contents/Frameworks/";
#endif
#else
    return "/usr/local/lib/" + std::string( MR_PROJECT_NAME ) + "/";
#endif
#endif
}

std::filesystem::path getUserConfigDir()
{
#if defined( _WIN32 )
    std::filesystem::path filepath( _wgetenv( L"APPDATA" ) );
#else
#if defined( __EMSCRIPTEN__ )
    std::filesystem::path filepath( "/" );
#else
    std::filesystem::path filepath;
    const auto* pw = getpwuid( getuid() );
    if ( pw )
    {
        filepath = pw->pw_dir;
    }
    else
    {
        spdlog::error( "getpwuid error! errno: {}", errno );
        filepath = std::getenv( "HOME" );
    }
#endif
    filepath /= ".local";
    filepath /= "share";
#endif
    filepath /= std::string( Config::instance().getAppName() );
    std::error_code ec;
    if ( !std::filesystem::is_directory( filepath, ec ) || ec )
    {
        if ( ec )
            spdlog::info( "{} is not a valid directory yet: {}", utf8string( filepath ), systemToUtf8( ec.message() ) );
        std::filesystem::create_directories( filepath, ec );
        if ( ec )
            spdlog::error( "create directories {} failed: {}", utf8string( filepath ), systemToUtf8( ec.message() ) );
    }
    return filepath;
}

std::filesystem::path getUserConfigFilePath()
{
    std::filesystem::path filepath = getUserConfigDir();
    filepath /= "config.json";
    return filepath;
}

std::filesystem::path GetTempDirectory()
{
    std::error_code ec;
    auto res = std::filesystem::temp_directory_path( ec );
    if ( ec )
        return {};
    res /= MR_PROJECT_NAME;

    if ( !std::filesystem::is_directory( res, ec ) )
    {
        ec.clear();
        if ( !std::filesystem::create_directories( res, ec ) )
            return {};
    }

    return res;
}

std::filesystem::path GetHomeDirectory()
{
#if defined( _WIN32 )
    if ( auto* home = _wgetenv( L"USERPROFILE" ) )
        return home;
#elif !defined( __EMSCRIPTEN__ )
    if ( auto* home = std::getenv( "HOME" ) )
        return home;
    if ( auto* pw = getpwuid( getuid() ) )
        return pw->pw_dir;
#endif
    return {};
}

std::string GetClipboardText()
{
#if defined( __EMSCRIPTEN__ )
    return "";
#elif defined( _WIN32 )
    // Try opening the clipboard
    if ( !OpenClipboard( nullptr ) )
    {
        spdlog::warn( "Could not open clipboard" );
        return "";
    }

    // Get handle of clipboard object for ANSI text
    HANDLE hData = GetClipboardData( CF_TEXT );
    if ( !hData )
    {
        // no text data
        CloseClipboard();
        return "";
    }

    // Lock the handle to get the actual text pointer
    char* pszText = static_cast< char* >( GlobalLock( hData ) );
    if ( !pszText )
    {
        CloseClipboard();
        return "";
    }
    // Save text in a string class instance
    std::string text( pszText );

    // Release the lock
    GlobalUnlock( hData );

    // Release the clipboard
    CloseClipboard();

    return text;
#elif defined(MRMESH_NO_CLIPBOARD)
    return "";
#else
    std::string text;
    if ( !clip::get_text( text ) )
    {
        spdlog::error( "Could not open clipboard" );
        return "";
    }
    return text;
#endif
}

void SetClipboardText( const std::string& text )
{
#if defined( __EMSCRIPTEN__ )
    ( void )text;
    return;
#elif defined( _WIN32 )
    HGLOBAL hMem = GlobalAlloc( GMEM_MOVEABLE, text.size() + 1 );
    memcpy( GlobalLock( hMem ), text.c_str(), text.size() + 1 );
    GlobalUnlock( hMem );
    OpenClipboard( 0 );
    EmptyClipboard();
    SetClipboardData( CF_TEXT, hMem );
    CloseClipboard();
#elif defined( MRMESH_NO_CLIPBOARD )
    ( void )text;
    return;
#else
    if ( !clip::set_text( text ) )
        spdlog::error( "Could not set clipboard" );
#endif
}

std::string GetMRVersionString()
{
#ifndef __EMSCRIPTEN__
    auto directory = GetResourcesDirectory();
    auto versionFilePath = directory / "mr.version";
    std::error_code ec;
    std::string configPrefix = "";
#ifndef NDEBUG
    configPrefix = "Debug: ";
#endif
    if ( !std::filesystem::exists( versionFilePath, ec ) )
        return configPrefix + "Version undefined";
    std::ifstream versFile( versionFilePath );
    if ( !versFile )
        return configPrefix + "Version reading error";
    std::string version;
    versFile >> version;
    if ( !versFile )
        return configPrefix + "Version reading error";
    return configPrefix + version;
#else
    auto *jsStr = (char *)EM_ASM_PTR({
        var version = "undefined";
        if ( typeof mrVersion != "undefined" )
            version = mrVersion;
        var lengthBytes = lengthBytesUTF8( version ) + 1;
        var stringOnWasmHeap = _malloc( lengthBytes );
        stringToUTF8( version, stringOnWasmHeap, lengthBytes );
        return stringOnWasmHeap;
    });
    std::string version( jsStr );
    free( jsStr );
    return version;
#endif
}

void OpenLink( const std::string& url )
{
#ifdef _WIN32
    ShellExecuteA( NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL );
#else
#ifdef __EMSCRIPTEN__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
    EM_ASM( open_link( UTF8ToString( $0 ) ), url.c_str() );
#pragma clang diagnostic pop
#else
#ifdef __APPLE__
    auto openres = system( ( "open " + url ).c_str() );
#else
    auto openres = system( ( "xdg-open " + url + " &" ).c_str() );
#endif
    if ( openres == -1 )
    {
        spdlog::warn( "Error opening {}", url );
    }
#endif
#endif // _WIN32
}

// Opens given file in associated application
bool OpenDocument( const std::filesystem::path& path )
{
#ifdef _WIN32
    HINSTANCE result = ShellExecuteW( NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    // "If the function succeeds, it returns a value greater than 32"
    if ( ( INT_PTR )result <= 32 )
    {
        spdlog::warn( "Error opening {}, error code {}", utf8string( path ), ( int )( INT_PTR )result );
        return false;
    }
    return true;

#else
#ifdef __EMSCRIPTEN__
    ( void )path;
    return false;
#else
    std::ostringstream command;
#ifdef __APPLE__
    command << "open " << std::quoted( path.string(), '\'' );
#else
    command << "xdg-open " << std::quoted( path.string(), '\'' ) << " &";
#endif
    auto openres = system( command.str().c_str() );
    if ( openres == -1 )
    {
        spdlog::warn( "Error opening {}", path.string() );
        return false;
    }
    return true;
#endif
#endif // _WIN32
}

#ifdef _WIN32
std::filesystem::path GetWindowsInstallDirectory()
{
    wchar_t szPath[MAX_PATH];
    if ( GetWindowsDirectoryW( szPath, MAX_PATH ) )
        return szPath;
    return "C:\\Windows";
}
#endif //_WIN32

std::string GetCpuId()
{
#ifdef __EMSCRIPTEN__
    return "Web Browser";
#elif defined(__APPLE__)
    char CPUBrandString[0x40] = {};
    size_t size = sizeof(CPUBrandString);
    if (sysctlbyname("machdep.cpu.brand_string", &CPUBrandString, &size, NULL, 0) < 0)
        spdlog::error("Apple sysctlbyname failed!");
    return CPUBrandString;
#elif defined(__ARM_CPU__)
    // TODO: https://stackoverflow.com/questions/64864035/any-cpuid-like-instruction-in-armv8
    return "ARM CPU";
#else
    // https://stackoverflow.com/questions/850774/how-to-determine-the-hardware-cpu-and-ram-on-a-machine
    char CPUBrandString[0x40] = {};
    int CPUInfo[4] = {-1};
    // Get the information associated with each extended ID.
#ifdef _MSC_VER
    __cpuid( CPUInfo, 0x80000000 );
#else
    __cpuid( 0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3] );
#endif
    unsigned nExIds = CPUInfo[0];
    for ( unsigned i = 0x80000000; i <= nExIds; ++i )
    {
#ifdef _MSC_VER
        __cpuid( CPUInfo, i );
#else
        __cpuid( i, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3] );
#endif
        // Interpret CPU brand string
        if ( i == 0x80000002 )
            std::memcpy( CPUBrandString, CPUInfo, sizeof( CPUInfo ) );
        else if ( i == 0x80000003 )
            std::memcpy( CPUBrandString + 16, CPUInfo, sizeof( CPUInfo ) );
        else if ( i == 0x80000004 )
            std::memcpy( CPUBrandString + 32, CPUInfo, sizeof( CPUInfo ) );
    }
    for ( int i = 0x3f; i >= 0; --i )
    {
        if ( CPUBrandString[i] == ' ' )
            CPUBrandString[i] = '\0';
        else if ( CPUBrandString[i] != '\0' )
            break;
    }

    auto res = std::string( CPUBrandString );
    return res.substr( res.find_first_not_of(' ') );
#endif
}

std::string GetDetailedOSName()
{
#ifdef _WIN32
    wchar_t value[255];
    DWORD BufferSize = 255;
    RegGetValue( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName",
        RRF_RT_ANY, NULL, ( PVOID )&value, &BufferSize );
    auto winName = Utf16ToUtf8( value );

    BufferSize = 255;
    RegGetValue( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuild",
    RRF_RT_ANY, NULL, ( PVOID )&value, &BufferSize );
    auto buildStr = Utf16ToUtf8( value );

    int build = std::atoi( buildStr.c_str() );
    if ( build >= 22000 )
    {
        auto winPos = winName.find( "Windows 10" );
        if ( winPos != std::string::npos )
            winName[winPos + 9] = '1';
    }
    winName += " " + buildStr;

    return winName;
#else
#ifdef __EMSCRIPTEN__
    return "Wasm";
#else
// if linux
#ifndef __APPLE__
    std::ifstream stream( "/etc/os-release" );
    std::string line;
    std::regex nameRegex( "^PRETTY_NAME=\"(.*?)\"$" );
    std::smatch match;

    std::string name;
    while ( std::getline( stream, line ) )
    {
        if ( std::regex_search( line, match, nameRegex ) )
        {
            name = match[1].str();
            break;
        }
    }
    return name;
#else // if  apple
    char buf[1024];
    unsigned buflen = 0;
    char line[256];
    FILE* sw_vers = popen( "sw_vers -productName", "r" );
    while ( fgets( line, sizeof( line ), sw_vers ) != NULL )
    {
        int l = snprintf( buf + buflen, sizeof( buf ) - buflen, "%s", line );
        buflen += l;
        assert( buflen < sizeof( buf ) );
    }
    pclose( sw_vers );
    sw_vers = popen( "sw_vers -productVersion", "r" );
    while ( fgets( line, sizeof( line ), sw_vers ) != NULL )
    {
        int l = snprintf( buf + buflen, sizeof( buf ) - buflen, " %s", line );
        buflen += l;
        assert( buflen < sizeof( buf ) );
    }
    pclose( sw_vers );
    auto aplStr = std::string( buf );
    aplStr.erase( std::remove( aplStr.begin(), aplStr.end(), '\n' ), aplStr.end() );
    return aplStr;
#endif
#endif
#endif
}

std::string getOSNoSpaces()
{
    #ifdef _WIN32
    return "Windows";
    #else
    #ifdef __EMSCRIPTEN__
    return "Wasm";
    #else
    // get platform from cmake variables
    #ifdef MR_PLATFORM
    std::string platform = MR_PLATFORM;
    std::replace(platform.begin(), platform.end(), ' ', '_');
    return platform;
    #else
    return "UNKNOWN";
    #endif
    #endif
    #endif
}

void setNewHandlerIfNeeded()
{
#ifdef __EMSCRIPTEN__
    std::set_new_handler( []
    {
        MAIN_THREAD_EM_ASM( notEnoughMemoryError() );
        // Default Emscripten behaviour if exceptions are disabled
        std::abort();
    } );
#endif
}

#ifndef __EMSCRIPTEN__
std::string getCurrentStacktrace()
{
    return getCurrentStacktraceInline();
}
#endif

#ifdef _WIN32
ProccessMemoryInfo getProccessMemoryInfo()
{
    ProccessMemoryInfo res;
    PROCESS_MEMORY_COUNTERS pmc;
    if ( GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof(pmc)) )
    {
        res.currVirtual =    pmc.PagefileUsage;
        res.maxVirtual = pmc.PeakPagefileUsage;
        res.currPhysical =    pmc.WorkingSetSize;
        res.maxPhysical = pmc.PeakWorkingSetSize;
    }
    return res;
}
#endif //_WIN32

} //namespace MR
