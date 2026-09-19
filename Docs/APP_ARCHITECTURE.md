# How the Classes Connect to `App.cpp`

`App.cpp` is the coordinator of the engine right now. It does not implement every feature itself. Instead, it creates the major systems, calls them in the correct order, and passes data between them.

A useful mental model is:

```text
Mara.cpp
    |
    v
App
    |
    +-- Window       creates the OpenGL window and context
    +-- Input        reads keyboard and mouse state
    +-- Shader       loads and uses GLSL programs
    +-- Model        loads a model through Assimp
    |      |
    |      +-- Mesh  stores and draws one piece of the model
    |             |
    |             +-- VAO/VBO/EBO store geometry on the GPU
    |             +-- Texture loads image data with STB
    |
    +-- ImGui        draws the editor interface
```

## 1. Program Entry: `Mara.cpp`

`Mara.cpp` contains `main()`:

```cpp
int main()
{
    Mara::App app;
    app.initialize();
    app.run();
    return 0;
}
```

This is the starting point of the program.

`main()` does not create the window or load the model directly. It creates one `App` object and asks the app to:

1. Initialize everything.
2. Start the main loop.
3. Clean itself up when the app is destroyed.

That keeps the program entry point small.

## 2. `App`: The Coordinator

`App` owns the main systems as smart pointers in `includes/App.h`:

```cpp
std::unique_ptr<Window> m_Window;
std::unique_ptr<Shader> m_Shader;
std::unique_ptr<Model> m_Model;
```

This means `App` owns these objects. When `App` is destroyed, the objects are destroyed too.

The other members are temporary application state:

```cpp
float m_ModelScale;
float m_CameraDistance;
float m_CameraYaw;
float m_CameraPitch;
glm::vec3 m_CameraTarget;
```

Eventually these values may move into a real `Camera` or editor state class. For now, keeping them in `App` makes the data flow visible.

## 3. Initialization Flow

`App::initialize()` creates the systems in the order they are needed.

### Window first

```cpp
m_Window = std::make_unique<Window>(900, 900, "Mara Engine");
m_Window->initialize();
m_Window->show();
```

`Window` is responsible for:

- Initializing GLFW.
- Creating the GLFW window.
- Creating the OpenGL context.
- Loading OpenGL functions through GLAD.
- Showing and later destroying the window.

After this finishes, OpenGL calls are safe to use.

### Input next

```cpp
MaraGl::Input::Init(m_Window->getWindow());
```

`Input` needs the raw `GLFWwindow*` so it can ask GLFW questions such as:

- Is the `W` key pressed?
- Is the right mouse button pressed?
- How far did the mouse move?
- Did the mouse wheel move?

`App` does not read GLFW input directly. It asks the `Input` class instead.

### Shader

```cpp
m_Shader = std::make_unique<Shader>(
    "shaders/triangle.vert",
    "shaders/triangle.frag");
```

Despite the old `triangle` filename, these are now the model shaders.

`Shader`:

1. Reads the vertex shader file.
2. Reads the fragment shader file.
3. Compiles both files.
4. Links them into one OpenGL shader program.
5. Provides functions such as `setMat4()` and `setVec3()` for sending values to GLSL.

The shader does not know about `Model` or `App`. It only receives vertex data and uniform values.

### Model

```cpp
m_Model = std::make_unique<Model>(
    "resources/models/robot/i-robot obj.obj");
```

`Model` uses Assimp to read the file. Assimp returns a scene containing nodes and meshes.

`Model` then:

1. Walks through the scene nodes recursively.
2. Converts each Assimp `aiMesh` into the engine's `Mesh` class.
3. Stores the meshes in `std::vector<Mesh>`.

The relationship is:

```text
Model = the whole imported file
Mesh  = one drawable piece inside that file
```

## 4. What Happens During One Frame

The main loop is in `App::run()`:

```cpp
while (!glfwWindowShouldClose(m_Window->getWindow()))
{
    Input::Update();
    updateCamera(deltaTime);
    renderFrame();
    glfwSwapBuffers(...);
    m_Window->pollEvents();
}
```

Each frame has four important stages.

### Stage 1: Read input

```cpp
MaraGl::Input::Update();
```

`Input::Update()` compares the current mouse position with the previous mouse position. That creates mouse deltas for camera orbiting.

The keyboard state is read when `updateCamera()` calls `Input::IsKeyPressed()`.

### Stage 2: Update camera state

```cpp
updateCamera(deltaTime);
```

This converts input into camera values:

- Right mouse drag changes yaw and pitch.
- Mouse wheel changes distance.
- `WASD` moves the camera target horizontally.
- `Q/E` moves the target vertically.

`App` owns these values currently because it is also the place where the view matrix is built.

### Stage 3: Render the model

`renderFrame()` creates three matrices:

```cpp
glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
```

They mean:

- **Model:** where the object is in the world.
- **View:** where the camera is and what it is looking at.
- **Projection:** how the 3D world is converted to the 2D screen.

`App` sends them to the shader:

```cpp
m_Shader->setMat4("model", model);
m_Shader->setMat4("view", view);
m_Shader->setMat4("projection", projection);
```

Then it asks the model to draw:

```cpp
m_Model->Draw(*m_Shader);
```

`Model::Draw()` loops over its meshes:

```cpp
for (Mesh &mesh : meshes)
{
    mesh.Draw(shader);
}
```

### Stage 4: Draw ImGui

After the 3D model is drawn, `App` starts a new ImGui frame:

```cpp
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplGlfw_NewFrame();
ImGui::NewFrame();
```

Then it creates the editor panel with calls such as:

```cpp
ImGui::Begin("Mara Editor");
ImGui::SliderFloat("Distance", &m_CameraDistance, 1.0f, 50.0f);
ImGui::End();
```

The important detail is the `&` before `m_CameraDistance`. ImGui receives the address of the value, so moving the slider changes the same camera value that `renderFrame()` uses.

Finally:

```cpp
ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

This sends the ImGui draw data to OpenGL.

## 5. How `Mesh` Reaches OpenGL

`Mesh` stores:

```cpp
std::vector<Vertex> vertices;
std::vector<GLuint> indices;
std::vector<Texture> textures;
```

When a mesh is first drawn, it creates:

- A `VAO`: remembers how vertex data is organized.
- A `VBO`: stores vertex data on the GPU.
- An `EBO`: stores index data on the GPU.

The vertex layout is configured in `Mesh::setupMesh()`:

```text
location 0 -> position
location 1 -> normal
location 2 -> texture coordinates
location 3 -> bone IDs
location 4 -> bone weights
```

The current shaders use position and normal. The other attributes are already reserved for future animation and texture work.

When `Mesh::Draw()` calls:

```cpp
glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
```

OpenGL draws that mesh using the currently active shader and the mesh's VAO/EBO.

## 6. How Lighting Connects

`App` sends light and camera values to the shader:

```cpp
m_Shader->setVec3("viewPos", cameraPosition);
m_Shader->setVec3("lightDirection", -1.0f, -1.0f, -1.0f);
m_Shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);
```

The vertex shader transforms positions and normals. The fragment shader uses the normal to calculate:

- Ambient light.
- Diffuse light based on the angle to the light.
- Specular highlights based on the camera direction.

The flow is:

```text
App values
    -> Shader uniforms
    -> GLSL lighting calculation
    -> final pixel color
```

## 7. External Libraries Versus Engine Classes

There are two kinds of code in this project.

### Engine code

These are your classes:

- `App`
- `Window`
- `Input`
- `Shader`
- `Model`
- `Mesh`
- `Texture`
- `VAO`, `VBO`, and `EBO`

### External libraries

These are integrated dependencies:

- GLFW: window creation and input backend.
- GLAD: OpenGL function loading.
- GLM: vector and matrix math.
- Assimp: model file importing.
- STB Image: image loading.
- Dear ImGui: editor UI.

Your classes act as a layer around the external libraries. For example:

```text
App -> Window -> GLFW
App -> Model -> Assimp
App -> Shader -> OpenGL
App -> Input -> GLFW
App -> ImGui backend -> GLFW/OpenGL
```

This wrapper layer is useful because `App` can talk to your own classes instead of knowing every detail of every library.

## 8. Build-Time Versus Runtime

`CMakeLists.txt` decides what is compiled and linked.

For example:

```cmake
target_link_libraries(ngine PRIVATE assimp imgui glfw glad)
```

That is build-time wiring. It tells the compiler and linker which libraries are available.

The resource copy commands make runtime files available beside the executable:

```cmake
COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:ngine>/resources
```

That is why `App.cpp` can use relative paths such as:

```cpp
"resources/models/robot/i-robot obj.obj"
"resources/fonts/Roboto-Variable.ttf"
```

## 9. A Simple Way to Trace the Code

When something in `App.cpp` is confusing, follow this sequence:

1. Find the member being used, such as `m_Model`.
2. Open its type in `App.h`.
3. Open that class's header, such as `Model.h`.
4. Find the method called by `App`, such as `Model::Draw()`.
5. Follow the next method call until you reach OpenGL or an external library.

For example:

```text
App::renderFrame()
    -> Model::Draw()
        -> Mesh::Draw()
            -> VAO::Bind()
            -> glDrawElements()
```

Or:

```text
App::initialize()
    -> Model constructor
        -> Assimp::Importer::ReadFile()
        -> Model::processNode()
        -> Model::processMesh()
```

This is the main pattern of the project: `App` starts the operation, and the specialized class performs the details.

## 10. Current Limitations

The current structure is intentionally simple. Some things will likely move into their own classes later:

- Camera values and movement could become a `Camera` class.
- Lighting values could become a `Light` class.
- ImGui panels could become editor panel classes.
- Model materials and textures could become a material system.
- Scene objects could become an entity or component system.

For now, keeping these values in `App` makes the full flow visible while the project is still being learned and explored.
