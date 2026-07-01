/* 定义解释器运行时环境 */

#ifndef FIREFLY_COMPILER_ENVIRONMENT_HPP
#define FIREFLY_COMPILER_ENVIRONMENT_HPP

#include "object.hpp"
#include <map>
#include <memory>
#include <string>
#include <utility>

using namespace std;

class Environment : public enable_shared_from_this<Environment> {
public:
    map<string, ObjectPtr> store;
    shared_ptr<Environment> outer;

    Environment() = default;

    explicit Environment(shared_ptr<Environment> outer) : outer(std::move(outer)) {}

    ObjectPtr get(const string &name) const {
        auto it = store.find(name);
        if (it != store.end()) {
            return it->second;
        }

        if (outer != nullptr) {
            return outer->get(name);
        }

        return nullptr;
    }

    ObjectPtr set(const string &name, ObjectPtr value) {
        store[name] = std::move(value);
        return store[name];
    }

    bool assign(const string &name, ObjectPtr value) {
        auto it = store.find(name);
        if (it != store.end()) {
            it->second = std::move(value);
            return true;
        }

        if (outer != nullptr) {
            return outer->assign(name, std::move(value));
        }

        return false;
    }
};

inline shared_ptr<Environment> new_environment() {
    return make_shared<Environment>();
}

inline shared_ptr<Environment> new_enclosed_environment(shared_ptr<Environment> outer) {
    return make_shared<Environment>(std::move(outer));
}

#endif //FIREFLY_COMPILER_ENVIRONMENT_HPP
