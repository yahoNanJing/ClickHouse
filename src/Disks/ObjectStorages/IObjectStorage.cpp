#include <Disks/IO/ThreadPoolRemoteFSReader.h>
#include <Disks/ObjectStorages/IObjectStorage.h>
#include <Disks/ObjectStorages/ObjectStorageIterator.h>
#include <IO/ReadBufferFromFileBase.h>
#include <IO/WriteBufferFromFileBase.h>
#include <IO/copyData.h>
#include <Interpreters/Context.h>
#include <Common/Exception.h>
#include <Common/ObjectStorageKeyGenerator.h>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int NOT_IMPLEMENTED;
    extern const int LOGICAL_ERROR;
}

std::string ObjectMetadata::toJson() const
{
    Poco::JSON::Object json;
    json.set("size_bytes", size_bytes);
    json.set("last_modified", last_modified.epochTime());
    json.set("etag", etag);

    Poco::JSON::Object attr_json;
    for (const auto & [key, value] : attributes)
    {
        attr_json.set(key, value);
    }
    json.set("attributes", attr_json);

    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(json, oss);
    return oss.str();
}

void ObjectMetadata::fromJson(const std::string & json_str)
{
    Poco::JSON::Parser parser;
    auto parsed = parser.parse(json_str);
    auto json = parsed.extract<Poco::JSON::Object::Ptr>();

    size_bytes = json->getValue<uint64_t>("size_bytes");
    last_modified = Poco::Timestamp(json->getValue<int64_t>("last_modified"));
    etag = json->getValue<std::string>("etag");

    if (json->has("attributes"))
    {
        auto attr_json = json->get("attributes").extract<Poco::JSON::Object::Ptr>();
        for (const auto & [key, value] : *attr_json)
        {
            attributes[key] = value.toString();
        }
    }
}

std::string RelativePathWithMetadata::toJson() const
{
    Poco::JSON::Object json;
    json.set("relative_path", relative_path);

    if (metadata)
    {
        json.set("metadata", metadata->toJson());
    }

    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(json, oss);
    return oss.str();
}

void RelativePathWithMetadata::fromJson(const std::string & json_str)
{
    Poco::JSON::Parser parser;
    auto parsed = parser.parse(json_str);
    auto json = parsed.extract<Poco::JSON::Object::Ptr>();

    relative_path = json->getValue<std::string>("relative_path");

    if (json->has("metadata"))
    {
        if (!metadata)
        {
            metadata = std::make_optional<ObjectMetadata>();
        }
        metadata->fromJson(json->getValue<std::string>("metadata"));
    }
    else
    {
        metadata.reset();
    }
}

std::string RelativePathWithMetadata::toJsonWithType() const
{
    Poco::JSON::Object json;
    json.set("type", "SimpleObjectInfo");
    json.set("content", toJson());

    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(json, oss);
    return oss.str();
}

const MetadataStorageMetrics & IObjectStorage::getMetadataStorageMetrics() const
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Method 'getMetadataStorageMetrics' is not implemented");
}

bool IObjectStorage::existsOrHasAnyChild(const std::string & path) const
{
    RelativePathsWithMetadata files;
    listObjects(path, files, 1);
    return !files.empty();
}

void IObjectStorage::listObjects(const std::string &, RelativePathsWithMetadata &, size_t) const
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "listObjects() is not supported");
}


ObjectStorageIteratorPtr IObjectStorage::iterate(const std::string & path_prefix, size_t max_keys) const
{
    RelativePathsWithMetadata files;
    listObjects(path_prefix, files, max_keys);

    return std::make_shared<ObjectStorageIteratorFromList>(std::move(files));
}

std::optional<ObjectMetadata> IObjectStorage::tryGetObjectMetadata(const std::string & path) const
{
    try
    {
        return getObjectMetadata(path);
    }
    catch (...)
    {
        return {};
    }
}

ThreadPool & IObjectStorage::getThreadPoolWriter()
{
    auto context = Context::getGlobalContextInstance();
    if (!context)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Global context not initialized");

    return context->getThreadPoolWriter();
}

void IObjectStorage::copyObjectToAnotherObjectStorage( // NOLINT
    const StoredObject & object_from,
    const StoredObject & object_to,
    const ReadSettings & read_settings,
    const WriteSettings & write_settings,
    IObjectStorage & object_storage_to,
    std::optional<ObjectAttributes> object_to_attributes)
{
    if (&object_storage_to == this)
        copyObject(object_from, object_to, read_settings, write_settings, object_to_attributes);

    auto in = readObject(object_from, read_settings);
    auto out = object_storage_to.writeObject(object_to, WriteMode::Rewrite, /* attributes= */ {}, /* buf_size= */ DBMS_DEFAULT_BUFFER_SIZE, write_settings);
    copyData(*in, *out);
    out->finalize();
}

const std::string & IObjectStorage::getCacheName() const
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "getCacheName is not implemented for object storage");
}

ReadSettings IObjectStorage::patchSettings(const ReadSettings & read_settings) const
{
    return read_settings;
}

WriteSettings IObjectStorage::patchSettings(const WriteSettings & write_settings) const
{
    return write_settings;
}

}
