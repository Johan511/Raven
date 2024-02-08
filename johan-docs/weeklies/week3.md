# 28/1 - 3/2
Progress
1. Scraped MOQT factory and provide MOQT object
2. Library User sets callback as an lambda, user can use lambda capture 
    obviously. We set context pointor as a fixed closure of whatever 
    we deem would be required. In case of connection callback the closure only
    contains the MOQT object whereas in case of stream callback it contains the
    MOQT object and the connection handler. Check `struct Stream Context` is moqt.hpp
3. Started writing client
