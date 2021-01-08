#!/usr/bin/env python3

import sys
from urllib.parse import urlparse

import yaml

with open(sys.argv[1], 'r') as f:
    doc = yaml.safe_load(f)

methods = ["get", "put", "post", "head", "delete"]


def decodeType2(x):
    t = None
    if x == "string":
        t = "std::string"
    if x == "number":
        t = "double"
    if x == "integer":
        t = "uint64_t"
    if t is None:
        raise Exception("unexpected type: " + str(x))
    return t


def decodeType(p):
    t = decodeType2(p['schema']['type'])
    if p.get('required', 'False') == 'True':
        return f"std::optional<{t}>";
    return t


def xname(x):
    return x.translate({ord(i): None for i in '/{}'})


def fname(p):
    return p['operationId'].replace('-', '_')

def cname(doc):
    s = doc['info']['title'] + '_' + doc['info']['version']
    s = s.replace('.','_')
    return s

def xpath(x):
    xr = []
    for n in x.split('/'):
        if n.find('{') != -1:
            break
        xr.append(n)
    return '/'.join(xr)


def parameters(p, p1, p2):
    print(f"struct {fname(p)}_parameters {{")
    for param in p1 + p2:
        xt = decodeType(param)
        xn = param['name']
        print(f"{xt} {xn} = {{}};")
    print(f"bool __bad_request = false;")
    print(f"{fname(p)}_parameters(const asio_http::Request& aRequest) {{")
    print(f"auto sQuery = aRequest.target();")
    print(f"std::string_view sQueryView(sQuery.data(), sQuery.size());")
    print(f"Parser::http_query(sQueryView, [this](auto aName, auto aValue) {{")
    for param in p2:  # query parameters
        xn = param['name']
        xt = param['schema']['type']
        if xt == 'string':
            print(f"if (aName == \"{xn}\") {{ this->{xn} = aValue; return; }}")
        elif xt == 'integer':
            print(f"if (aName == \"{xn}\") {{ this->{xn} = Parser::Atoi<uint64_t>(aValue); return; }}")
        elif xt == 'number':
            print(f"if (aName == \"{xn}\") {{ this->{xn} = Parser::Atof<double>(aValue); return; }}")
    print(f"this->__bad_request = true;")
    print(f"}});")
    print(
        f"Parser::http_path_params(sQueryView, \"{urlparse(doc['servers'][0]['url'] + path).path}\" ,[this](auto aName, auto aValue){{ ")
    for param in p1:  # path parameters
        xn = param['name']
        print(f"if (aName == \"{xn}\") {{ this->{xn} = aValue; return; }}")
    print(f"this->__bad_request = true;")
    print(f"}});")
    # FIXME: ensure required parameters set
    print(f"}} // {fname(p)}_parameters c-tor")
    print(f"}};")


def body(p):
    bodyBinary = False
    print(f"struct {fname(p)}_body {{")
    for ct in p.get('requestBody', {}).get('content', {}):
        if ct == 'application/octet-stream':
            bodyBinary = True
    if bodyBinary:
        print(f"std::string body;")
    print(f"{fname(p)}_body(const asio_http::Request& aRequest) {{")
    if bodyBinary:
        print(f"body = aRequest.body();")
    print(f"}}")
    print(f"}};")


def response_vars(ct):
    if ct['type'] == 'string':
        print(f"std::string response;");
    if ct['type'] == 'array':
        print(f"std::vector<{decodeType2(ct['items']['type'])}> response;");
    if ct['type'] == 'object':
        for element in ct['properties']:
            xe = ct['properties'][element]
            print(f"std::optional<{decodeType2(xe['type'])}> {element};")


def response_json(ct):
    if ct['type'] == 'array':
        print(f"sValue = Format::Json::to_value(response);")
    if ct['type'] == 'object':
        for element in ct['properties']:
            xe = ct['properties'][element]
            print(f"sValue[\"{element}\"] = Format::Json::to_value({element});")


def response(p):
    responseIsJson = False
    responseIsBinary = False
    print(f"struct {fname(p)}_response {{")
    vt = []
    for code in p['responses']:
        if 'content' not in p['responses'][code]:
            continue
        content = p['responses'][code]['content']
        for ct in content:
            if ct == "application/octet-stream":
                responseIsBinary = True
                vt.append(content[ct]['schema'])
                break
            if ct == "application/json":
                responseIsJson = True
                vt.append(content[ct]['schema'])
    for n in vt:
        response_vars(n)
    print(f"void result(asio_http::Response& aResponse) {{")
    print(f"namespace http = boost::beast::http;")
    if responseIsBinary:
        print(f"aResponse.set(http::field::content_type, \"application/octet-stream\");")
        print(f"aResponse.body().append(response);")
    if responseIsJson:
        print(f"::Json::Value sValue;")
        for n in vt:
            response_json(n)
        print(f"aResponse.set(http::field::content_type, \"application/json\");")
        print(f"aResponse.body().append(Format::Json::to_string(sValue));")
    print(f"}} ")
    print(f"}};")


def call(p):
    print(f"void {fname(p)}")
    print(
        f"(asio_http::asio::io_service& aService, const asio_http::Request& aRequest, asio_http::Response& aResponse, asio_http::asio::yield_context yield) {{")
    print(f"namespace http = boost::beast::http;")
    print(f"const {fname(p)}_parameters sParameters(aRequest);")
    print(f"if (sParameters.__bad_request) {{ aResponse.result(http::status::bad_request); return; }}")
    print(f"const {fname(p)}_body sBody(aRequest);")
    print(f"auto [sStatus, sResponse] = {fname(p)}_i(aService, sParameters, sBody, yield);")
    print(f"aResponse.result(sStatus);")
    print(f"sResponse.result(aResponse);")
    print(f"}}")


print(f"#include <parser/Atoi.hpp>")
print(f"#include <asio_http/Router.hpp>")
print(f"#include <format/Json.hpp>")
print(f"namespace api {{")
print(f"struct {cname(doc)} {{")

for path in doc['paths']:
    for method in doc['paths'][path]:
        if method not in methods:
            continue
        p = doc['paths'][path][method]
        parameters(p, doc['paths'][path].get('parameters', []), p.get('parameters', []))
        body(p)
        response(p)
        call(p)

print(f"void configure(asio_http::RouterPtr aRouter) {{")
print(f"namespace http = boost::beast::http;")
for path in doc['paths']:
    url = urlparse(doc['servers'][0]['url'] + path)
    print(f"aRouter->insert(\"{xpath(url.path)}\",")
    print(
        f"[this](asio_http::asio::io_service& aService,"
        f"const asio_http::Request& aRequest,"
        f"asio_http::Response& aResponse,"
        f"asio_http::asio::yield_context yield) ")
    print(f"{{")
    print(f"switch (aRequest.method()) {{")
    for method in doc['paths'][path]:
        if method not in methods:
            continue
        p = doc['paths'][path][method]
        if method == "delete":
            method += "_"
        print(f"case http::verb::{method}: ")
        print(f"{fname(p)}(aService, aRequest, aResponse, yield); break;")
    print(f"default: aResponse.result(http::status::method_not_allowed);");
    print(f"}}")
    print(f"}});")
print(f"}}")

for path in doc['paths']:
    for method in doc['paths'][path]:
        if method not in methods:
            continue
        p = doc['paths'][path][method]
        print(
            f"virtual std::pair<boost::beast::http::status, {fname(p)}_response> {fname(p)}_i(asio_http::asio::io_service& aService,"
            f"const {fname(p)}_parameters& aRequest,"
            f"const {fname(p)}_body& aBody,"
            f"asio_http::asio::yield_context yield) = 0;")
print(f"virtual ~{cname(doc)}() {{}}")
print(f"}};")
print(f"}} // namespace api")
