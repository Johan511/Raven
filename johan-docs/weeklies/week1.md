# 15/1 - 20/1
Progress
1. CMake setup
2. RAII wrappers for table, listener, configuration, registration (stream is pending)
3. Read about QUIC / msquic => made session.md doc
4. started with src/server.cpp => 
            ran into issue with not being able to use lamdas to capture context
            Tried type erasure, could not make it work
            Can use context object => issue is API usability (go with flow and think of it later)
            Or can try (https://www.youtube.com/watch?v=z-kUhwANrIw&t=48s)

TODOs : 
1. Consider scraping MOQTFactory and only providing MOQT object with start function
    requires changes to wrappers => should be able to open and start seperately (more constructors and add start function)
2. const correctness
3. (Point 4 in previous section)
