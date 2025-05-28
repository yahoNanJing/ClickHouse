#pragma once

#include <Disks/ObjectStorages/IObjectStorage.h>

namespace DB
{

class ObjectInfoFactory final : private boost::noncopyable
{
public:
    using ObjectInfo = RelativePathWithMetadata;
    using ObjectInfoPtr = std::shared_ptr<ObjectInfo>;
    using SimpleCreator = std::function<ObjectInfoPtr()>;

    static ObjectInfoFactory & instance();

    ObjectInfoPtr get(const String & full_name) const;

    void registerObjectInfoType(const String & full_name, SimpleCreator creator);

private:
    ObjectInfoFactory();

private:
    std::unordered_map<String, SimpleCreator> object_info_creators;
};

void registerSimpleObjectInfo(ObjectInfoFactory & factory);
void registerIcebergObjectInfo(ObjectInfoFactory & factory);

std::shared_ptr<RelativePathWithMetadata> getObjectInfoFromJsonStr(const std::string & json_str);

}
