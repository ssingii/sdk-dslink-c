#include <dslink/storage/storage.h>
#include <dslink/storage/json_in_memory.h>

StorageProvider *dslink_storage_init(json_t *config) {
    (void) config;

    json_t *jPath = json_object_get(config, "path");
    char *path;

    if (json_is_string(jPath)) {
        path = (char *) json_string_value(jPath);
    } else {
        path = ".";
    }

    return dslink_storage_json_memory_create(path);
}

void dslink_storage_destroy(StorageProvider *provider) {
    provider->destroy_cb(provider);
}

void dslink_storage_push(StorageProvider *provider, char **key, json_t *value, storage_push_done_cb cb, void *data) {
    provider->push_cb(provider, key, value, cb, data);
}

void dslink_storage_pull(StorageProvider *provider, char **key, storage_pull_done_cb cb, void *data) {
    provider->pull_cb(provider, key, cb, data);
}

void dslink_storage_store(StorageProvider *provider, char **key, json_t *value, storage_store_done_cb cb, void *data) {
    provider->store_cb(provider, key, value, cb, data);
}

void dslink_storage_recall(StorageProvider *provider, char **key, storage_recall_done_cb cb, void *data) {
    provider->recall_cb(provider, key, cb, data);
}

json_t *dslink_storage_traverse(StorageProvider *provider) {
    return provider->traverse_cb(provider);
}