#include "http/servlets/config_servlet.hpp"

#include <jsoncpp/json/json.h>

#include "config/config.hpp"
#include "util/json_util.hpp"

namespace CIM::http {
ConfigServlet::ConfigServlet() : Servlet("ConfigServlet") {}

int32_t ConfigServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response,
                              HttpSession::ptr session) {
    std::string type = request->getParam("type");
    if (type == "json") {
        response->setHeader("Content-Type", "text/json charset=utf-8");
    } else {
        response->setHeader("Content-Type", "text/yaml charset=utf-8");
    }
    YAML::Node node;
    Config::Visit([&node](ConfigVariableBase::ptr base) {
        YAML::Node n;
        try {
            n = YAML::Load(base->toString());
        } catch (...) {
            return;
        }
        node[base->getName()] = n;
        node[base->getName() + "$description"] = base->getDescription();
    });
    if (type == "json") {
        Json::Value jvalue;
        if (YamlToJson(node, jvalue)) {
            response->setBody(JsonUtil::ToString(jvalue));
            return 0;
        }
    }
    std::stringstream ss;
    ss << node;
    response->setBody(ss.str());
    return 0;
}

}  // namespace CIM::http