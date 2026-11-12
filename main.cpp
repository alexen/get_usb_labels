#include <iostream>
#include <string>
#include <sstream>
#include <cctype>
#include <climits>
#include <cstdlib>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/fstream.hpp>

// Функция декодирования hex-escapes (\x20)
std::string decodeHexEscapes( const std::string& input )
{
     static auto hexVal = []( const char c ) -> int
     {
          if( c >= '0' && c <= '9' )
          {
               return c - '0';
          }
          if( c >= 'a' && c <= 'f' )
          {
               return c - 'a' + 10;
          }
          if( c >= 'A' && c <= 'F' )
          {
               return c - 'A' + 10;
          }
          return -1;
     };

     std::string result;
     result.reserve( input.length() );

     for( auto i = 0u; i < input.length(); ++i )
     {
          if( input[ i ] == '\\'
               && (i + 3) < input.length()
               && input[ i + 1 ] == 'x' )
          {
               const auto hi = hexVal( input[ i + 2 ] );
               const auto lo = hexVal( input[ i + 3 ] );
               if( hi >= 0 && lo >= 0 )
               {
                    result += static_cast< char >( (hi << 4) | lo );
                    i += 3;
                    continue;
               }
          }
          result += input[ i ];
     }

     return result;
}


std::string getReadableSize( const std::string& devName, const std::size_t kilo = 1000 )
{
     std::string parentName = devName;
     while( !parentName.empty() && std::isdigit( parentName.back() ) )
     {
          parentName.pop_back();
     }

     static const boost::filesystem::path sysClassBlockDir = "/sys/class/block/";
     boost::filesystem::ifstream sizeFile( sysClassBlockDir / parentName / "size" );
     long long sectors = 0;
     if( !(sizeFile >> sectors) )
     {
          return "?? GB";
     }

     static constexpr auto sectorSize = 512.0;
     static const std::string suffixes[] = { "B", "KB", "MB", "GB", "TB" };

     auto bytes = sectors * sectorSize;
     auto suffixIdx = 0u;

     while( bytes >= kilo && suffixIdx < 4 )
     {
          ++suffixIdx;
          bytes /= kilo;
     }

     return std::to_string( static_cast< std::size_t >( bytes ) )
          + ' '
          + suffixes[ suffixIdx ];
}


std::string getDeviceModel( const std::string& devName )
{
     std::string parentName = devName;
     while( !parentName.empty() && std::isdigit( parentName.back() ) )
     {
          parentName.pop_back();
     }

     static const boost::filesystem::path sysClassBlockDir = "/sys/class/block/";
     static const auto readParamFromFile = []( const boost::filesystem::path& baseDir, const std::string& param )
     {
          std::string value;
          boost::filesystem::ifstream ifile( baseDir / param );
          std::getline( ifile, value );
          boost::algorithm::trim( value );
          return value;
     };

     const auto basePath = sysClassBlockDir / parentName / "device";

     const auto vendor = readParamFromFile( basePath, "vendor" );
     const auto model = readParamFromFile( basePath, "model" );

     std::string summary = "Unknown device";
     if( !vendor.empty() )
     {
          summary = vendor;
     }
     if( !model.empty() )
     {
          summary += ' ' + model;
     }
     return summary;
}


// Получение метки для конкретного устройства (например, sdc1)
std::string findLabelForDevice( const std::string& targetDev )
{
     const boost::filesystem::path targetLabelDir = "/dev/disk/by-label";
     boost::system::error_code ec;

     if( boost::filesystem::exists( targetLabelDir, ec ) && !ec )
     {
          boost::filesystem::directory_iterator entry( targetLabelDir, ec );
          boost::filesystem::directory_iterator end;

          while( !ec && entry != end )
          {
               const auto symStat = entry->symlink_status( ec );
               if( !ec && symStat.type() == boost::filesystem::symlink_file )
               {
                    const auto devName = boost::filesystem::canonical( entry->path(), ec ).filename();
                    if( !ec && devName == targetDev )
                    {
                         return decodeHexEscapes( entry->path().filename().string() );
                    }
               }
               entry.increment( ec );
          }
     }
     return {};
}


// Получение информации о диске по его UUID
std::string getDiskInfoByUuid( const std::string& uuid )
{
     static const boost::filesystem::path diskUuidDir = "/dev/disk/by-uuid";

     const auto fullUuidPath = diskUuidDir / uuid;

     // Проверяем, существует ли такой UUID в системе
     boost::system::error_code ec;
     if( !boost::filesystem::exists( fullUuidPath, ec ) || ec )
     {
          return uuid;
     }

     // Определяем имя устройства по симлинку (например, "sdc1")
     const auto devName = boost::filesystem::canonical( fullUuidPath, ec ).filename().string();
     if( ec || devName.empty() )
     {
          return uuid;
     }

     // Собираем железные данные
     const auto model = getDeviceModel( devName );
     const auto size = getReadableSize( devName );
     const auto label = findLabelForDevice( devName );

     // Форматируем строку вывода
     std::ostringstream oss;
     oss << model << ", " << size;
     if( !label.empty() )
     {
          oss << " (" << label << ")";
     }

     return oss.str();
}


int main( int argc, char* argv[] )
{
     // Проверяем, передан ли хотя бы один аргумент
     if( argc < 2 )
     {
          std::cout << "Использование: " << argv[ 0 ] << " [UUID_1] [UUID_2] [UUID_3] ...\n";
          std::cout << "Пример: " << argv[ 0 ] << " 7BF7-B6CC 1A2B-3C4D\n";
          return 1;
     }

     // Последовательно перебираем все переданные аргументы
     for( auto i = 1; i < argc; ++i )
     {
          const std::string inputUuid = argv[ i ];
          const auto result = getDiskInfoByUuid( inputUuid );

          // Выводим UUID и результат его обработки
          std::cout << inputUuid << " -> " << result << "\n";
     }

     // Возвращаем 0, если все диски нашлись успешно, или 1, если была хоть одна
     // ошибка
     return 0;
}
