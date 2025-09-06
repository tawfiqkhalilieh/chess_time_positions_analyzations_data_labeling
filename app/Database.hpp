#pragma once
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <string>

class Database {
public:
    static Database& getInstance() {
        static Database instance; // Singleton instance
        return instance;
    }

    mongocxx::client& getClient() { return client; }
    mongocxx::database getDatabase(const std::string& name) { return client[name]; }
    mongocxx::collection getCollection(const std::string& dbName, const std::string& collName) {
        return client[dbName][collName];
    }

private:
    Database()
        : instance{}, 
          client(mongocxx::uri{"mongodb://root:example@mongo:27017/"}) 
    {}

    ~Database() = default;

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    mongocxx::instance instance;
    mongocxx::client client;
};
