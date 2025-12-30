#ifndef OPENGL_CONTEXT_H
#define OPENGL_CONTEXT_H

struct GLFWwindow;

class OpenGLContext {
public:
    OpenGLContext();
    ~OpenGLContext();

    bool init(int width, int height, const char* title);
    void cleanup();
    
    GLFWwindow* getWindow() const;

private:
    GLFWwindow* window;
};

#endif // OPENGL_CONTEXT_H 