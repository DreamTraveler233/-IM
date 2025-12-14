wrk.method = "POST"
wrk.body = [[{"hello": "world"}]]
wrk.headers["Content-Type"] = "application/json"

function request()
    return wrk.format(nil, nil, nil, wrk.body)
end
