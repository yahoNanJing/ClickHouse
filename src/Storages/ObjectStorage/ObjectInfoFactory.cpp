#include <Storages/ObjectStorage/ObjectInfoFactory.h>

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int UNKNOWN_TYPE;
}

ObjectInfoFactory & ObjectInfoFactory::instance()
{
    static ObjectInfoFactory ret;
    return ret;
}

ObjectInfoFactory::ObjectInfoPtr ObjectInfoFactory::get(const String & full_name) const
{
    auto it = object_info_creators.find(full_name);
    if (it == object_info_creators.end())
        throw Exception(ErrorCodes::UNKNOWN_TYPE, "ObjectInfoFactory: the object info type '{}' is unknown or not registered", full_name);

    const auto & creator = it->second;
    if (!creator)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "ObjectInfoFactory: the object info type '{}' has a null constructor", full_name);

    return creator();
}

void ObjectInfoFactory::registerObjectInfoType(const String & full_name, SimpleCreator creator)
{
    if (creator == nullptr)
        throw Exception(
            ErrorCodes::LOGICAL_ERROR, "ObjectInfoFactory: the object info type {} is provided with a null constructor", full_name);

    if (!object_info_creators.emplace(full_name, creator).second)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "ObjectInfoFactory: the object info type '{}' is not unique", full_name);
}

ObjectInfoFactory::ObjectInfoFactory()
{
    registerSimpleObjectInfo(*this);
    registerIcebergObjectInfo(*this);
}

void registerSimpleObjectInfo(ObjectInfoFactory & factory)
{
    factory.registerObjectInfoType("SimpleObjectInfo", []() { return std::make_shared<ObjectInfoFactory::ObjectInfo>(); });
}

ObjectInfoFactory::ObjectInfoPtr getObjectInfoFromJsonStr(const std::string & json_str)
{
    Poco::JSON::Parser parser;
    auto parsed = parser.parse(json_str);
    auto json = parsed.extract<Poco::JSON::Object::Ptr>();

    std::string type = json->getValue<std::string>("type");
    auto object_info = ObjectInfoFactory::instance().get(type);

    object_info->fromJson(json->getValue<std::string>("content"));

    return object_info;
}

}
