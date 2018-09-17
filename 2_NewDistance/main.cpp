#include <cmath>
// SOURCE FILES
#include "common.h"
#include "quadtree.h"
#include "mesh_utils.h"
#include "mesh.h"

// MACROS
#define LOG(fmt, ...)  fprintf(stdout, fmt, ##__VA_ARGS__); fflush(stdout);
#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

////////////////////////////////////////////////////////////////////////////////
///
/// Const and Structs
///
static const GLfloat black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
static const GLfloat grey[] = { 0.05f, 0.05f, 0.1f, 1.0f };
static const GLfloat one = 1.0;
static const mat4 IDENTITY = mat4(1.0f);

struct OpenGLApp {
    int gui_width, gui_height;

    bool lbutton_down, rbutton_down;
    double x0, y0;

    uint mode;
    string filepath;
    const string default_filepath = "bigguy.obj";

    bool auto_lod;
    float light_pos[3] = {50,-50,100};

    Mesh mesh;
    CameraManager cam;
} app = {};

struct BenchStats {
    double last_t;
    double current_t;
    double delta_T;

    double avg_qt_gpu_compute, avg_qt_gpu_render;
    double  total_qt_gpu_compute, total_qt_gpu_render;
    double  avg_frame_dt, total_frame_dt;

    int frame_count, real_fps;
    double sec_timer;
    int last_frame_count;

    void Init();
    void UpdateTime();
    void UpdateStats();
} bench = {};


////////////////////////////////////////////////////////////////////////////////
///
/// Update render paramenters
///
///

void updateRenderParams()
{
    app.mesh.quadtree->UpdateLightPos(vec3(app.light_pos[0], app.light_pos[1], app.light_pos[2]));
    app.mesh.quadtree->UpdateMode(app.mode);
    app.mesh.quadtree->UpdateScreenRes(std::max(app.cam.render_height, app.cam.render_width));
}

////////////////////////////////////////////////////////////////////////////////
///
/// Benchmarking Functions
///

void BenchStats::Init()
{
    current_t = glfwGetTime() * 0.001f;
    last_t = current_t;
    delta_T = 0;

    avg_qt_gpu_compute = 0;
    avg_qt_gpu_render = 0;
    avg_frame_dt = 0;
    frame_count = 0;
    total_qt_gpu_compute = 0;
    total_qt_gpu_render = 0;
    total_frame_dt = 0;
    sec_timer = 0;
    real_fps = 0;
    last_frame_count = 0;
}

void BenchStats::UpdateTime()
{
    current_t = glfwGetTime();
    delta_T = current_t - last_t;
    last_t = current_t;
}

void BenchStats::UpdateStats()
{
    frame_count++;
    sec_timer += delta_T;
    if (sec_timer < 1.0f) {
        total_qt_gpu_compute += app.mesh.quadtree->ticks.gpu_compute;
        total_qt_gpu_render += app.mesh.quadtree->ticks.gpu_render;
        total_frame_dt += delta_T;
    } else {
        real_fps = frame_count - last_frame_count;
        last_frame_count = frame_count;
        avg_qt_gpu_compute = total_qt_gpu_compute / double(real_fps);
        avg_qt_gpu_render = total_qt_gpu_render / double(real_fps);
        avg_frame_dt = total_frame_dt / double(real_fps);
        total_qt_gpu_compute = 0;
        total_qt_gpu_render = 0;
        total_frame_dt = 0;
        sec_timer = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////
///
/// GUI Functions
///

// Small trick for better timing output
void ImGuiTime(string s, float tmp)
{
    ImGui::Text("%s:  %.5f %s\n",
                s.c_str(),
                (tmp < 1. ? tmp * 1e3 : tmp),
                (tmp < 1. ? " ms" : " s"));
}

void RenderImgui()
{

    QuadTree::Settings& settings_ref = app.mesh.quadtree->settings;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(app.gui_width, app.gui_height));
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::Begin("Benchmark and Controls", nullptr, window_flags);
    {
        static float values_gpu_compute[80] = { 0 };
        static float values_gpu_render[80]  = { 0 };
        static float values_frame_dt[80] = { 0 };
        static float values_fps[80] = { 0 };

        static int offset = 0;
        static float refresh_time = 0;
        static float max_gpu_compute = 0, max_gpu_render = 0;
        static float max_fps = 0, max_dt = 0;

        static float array_max, current_val;

        if (refresh_time == 0)
            refresh_time = ImGui::GetTime();

        while (refresh_time < ImGui::GetTime())
        {
            values_gpu_compute[offset] = app.mesh.quadtree->ticks.gpu_compute * 1000.0f;
            values_gpu_render[offset]  = app.mesh.quadtree->ticks.gpu_render  * 1000.0f;
            values_frame_dt[offset] = bench.delta_T * 1000.0f;
            values_fps[offset] = ImGui::GetIO().Framerate;

            offset = (offset+1) % IM_ARRAYSIZE(values_gpu_compute);
            refresh_time += 1.0f/30.0f;
        }

        // QUADTREE COMPUTE DT
        current_val = app.mesh.quadtree->ticks.gpu_compute * 1000.0;
        array_max = *std::max_element(values_gpu_compute, values_gpu_compute+80);
        if (array_max > max_gpu_compute || array_max < 0.2 * max_gpu_compute)
            max_gpu_compute = array_max;
        ImGui::PlotLines("GPU compute dT", values_gpu_compute, IM_ARRAYSIZE(values_gpu_compute), offset,
                         std::to_string(current_val).c_str(), 0.0f, max_gpu_compute, ImVec2(0,80));

        //QUADTREE RENDER DT
        current_val = app.mesh.quadtree->ticks.gpu_render * 1000.0;
        array_max = *std::max_element(values_gpu_render, values_gpu_render+80);
        if (array_max > max_gpu_render || array_max < 0.2 * max_gpu_render)
            max_gpu_render = array_max;
        ImGui::PlotLines("GPU render dT", values_gpu_render, IM_ARRAYSIZE(values_gpu_render), offset,
                         std::to_string(current_val).c_str(), 0.0f, max_gpu_render, ImVec2(0,80));

        // FPS
        current_val = 1.0 / bench.delta_T;
        array_max = *std::max_element(values_fps, values_fps+80);
        if (array_max > max_fps || array_max < 0.2 * max_fps) max_fps = array_max;
        ImGui::PlotLines("FPS", values_fps, IM_ARRAYSIZE(values_fps), offset,
                         std::to_string(current_val).c_str(), 0.0f, max_fps, ImVec2(0,80));

        // dT
        current_val = bench.delta_T * 1000.0f;
        array_max = *std::max_element(values_frame_dt, values_frame_dt+80);
        if (array_max > max_dt || array_max < 0.2 * max_dt) max_dt = array_max;
        ImGui::PlotLines("Frame dT", values_frame_dt, IM_ARRAYSIZE(values_frame_dt), offset,
                         std::to_string(current_val).c_str(), 0.0f, max_dt, ImVec2(0,80));

//#define CLEAN

#ifndef CLEAN
        ImGui::Text("\nOutput FPS (1s) %d", bench.real_fps);
        ImGuiTime("avg GPU Compute dT (1s)", bench.avg_qt_gpu_compute);
        ImGuiTime("avg GPU Render  dT (1s)", bench.avg_qt_gpu_render);
        ImGuiTime("avg Frame dT (1s)      ", bench.avg_frame_dt);
        ImGui::Text("\n");
#endif

        if (ImGui::Combo("Mode", (int*)&app.mode, "Terrain\0Mesh\0\0")) {
            app.mesh.CleanUp();
            app.cam.Init(app.mode);
            app.mesh.Init(app.mode, app.cam, app.filepath);
            updateRenderParams();
            app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
            app.mesh.quadtree->UploadSettings();
        }
        ImGui::Text("\n");

        static bool advanced = false;
        if (ImGui::Checkbox("Advanced Mode", &advanced))
            settings_ref.map_nodecount = advanced;
        if (advanced)
        {
            ImGui::Text("\n------ Renderer Settings ------\n");
            if (ImGui::Checkbox("Render Projection", &settings_ref.projection_on)) {
                app.mesh.quadtree->UploadSettings();
            }
            if (ImGui::SliderFloat("FOV", &app.cam.fov, 5, 90)) {
                app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
                app.mesh.UpdateForFOV(app.cam);
                app.mesh.quadtree->UploadSettings();
            }
            if (ImGui::Button("Reinit Camera")) {
                app.cam.Init(app.mode);
                app.mesh.InitTransforms(app.cam);
                app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
                app.mesh.quadtree->UploadSettings();
            }

            if (ImGui::Checkbox("Wireframe", &settings_ref.wireframe_on)) {
                app.mesh.quadtree->ReloadRenderProgram();
                updateRenderParams();
            }
            if(!settings_ref.wireframe_on){
            ImGui::SameLine();
            if (ImGui::Checkbox("Flat Normals", &settings_ref.flat_normal)) {
                app.mesh.quadtree->ReloadRenderProgram();
                updateRenderParams();
            } }
            if (ImGui::Combo("Color mode", &settings_ref.color_mode,
                             "LoD & Morph\0White Wireframe\0Polygone Highlight\0Frustum\0Cull\0Debug\0\0")) {
                app.mesh.quadtree->UploadSettings();
            }
            if(ImGui::DragFloat3("Light pos", app.light_pos, 0.1f)) {
                app.mesh.quadtree->UpdateLightPos(vec3(app.light_pos[0], app.light_pos[1], app.light_pos[2]));
            }

            ImGui::Text("\n------ Mesh Settings ------\n");

            if (app.mode == TERRAIN){
                if (ImGui::Checkbox("Displacement Mapping", &settings_ref.displace_on)) {
                    app.mesh.quadtree->ReloadShaders();
                    app.mesh.quadtree->UploadSettings();
                    updateRenderParams();
                }
            }
            if(settings_ref.displace_on){
                if (ImGui::SliderFloat("Height Factor", &settings_ref.displace_factor, 0, 2)) {
                    app.mesh.quadtree->UploadSettings();
                }
            }
            if (ImGui::Checkbox("Rotate Mesh", &settings_ref.rotateMesh)) {
                app.mesh.quadtree->UploadSettings();
            }
            if (ImGui::Checkbox("Uniform", &settings_ref.uniform_on)) {
                app.mesh.quadtree->UploadSettings();
            }
            ImGui::SameLine();
            if (ImGui::SliderInt("", &settings_ref.uniform_lvl, 0, 20)) {
                app.mesh.quadtree->UploadSettings();
            }
            ImGui::Checkbox("Auto LoD", &app.auto_lod);
            float expo = log2(settings_ref.target_e_length);
            if (ImGui::SliderFloat("Edge Length (2^x)", &expo, 1.0f, 10.0f)) {
                settings_ref.target_e_length = std::pow(2.0f, expo);
                app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
                app.mesh.quadtree->UploadSettings();
            }
            if (ImGui::Checkbox("Readback node count", &settings_ref.map_nodecount)) {
                app.mesh.quadtree->UploadSettings();
            }
            if (settings_ref.map_nodecount) {
                int leaf_tri = (1<<(settings_ref.cpu_lod*2));
                ImGui::Text("Total    : "); ImGui::SameLine();
                ImGui::Text("%s", utility::LongToString(app.mesh.quadtree->full_node_count).c_str());
                ImGui::Text("Drawn    : "); ImGui::SameLine();
                ImGui::Text("%s", utility::LongToString(app.mesh.quadtree->drawn_node_count).c_str());
                ImGui::Text("Triangles: "); ImGui::SameLine();
                ImGui::Text("%s", utility::LongToString(app.mesh.quadtree->drawn_node_count*leaf_tri).c_str());
            }
            if (ImGui::Combo("Polygon type", &settings_ref.polygon_type, "Triangle\0Quad\0\0")) {
                app.mesh.LoadMeshBuffers();
                app.mesh.quadtree->Reinitialize();
                updateRenderParams();
            }
            if (ImGui::SliderInt("CPU LoD", &settings_ref.cpu_lod, 0, 4)) {
                app.mesh.quadtree->Reinitialize();
                app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
                app.mesh.quadtree->UploadSettings();
                updateRenderParams();

            }
            if (ImGui::Checkbox("Cull", &settings_ref.cull_on)) {
                app.mesh.quadtree->ReloadComputeProgram();
                app.mesh.quadtree->UploadSettings();
                updateRenderParams();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Freeze", &settings_ref.freeze)) {
                app.mesh.quadtree->ReconfigureShaders();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reinitialize QuadTree")) {
                app.mesh.quadtree->Reinitialize();
                updateRenderParams();
            }
            if (app.mode == MESH) {
                if (ImGui::Combo("Interpolation type", &settings_ref.itpl_type,
                                 "Linear\0PN Triangles\0Phong\0\0\0")) {
                    app.mesh.quadtree->ReloadRenderProgram();
                    updateRenderParams();
                }
                if (ImGui::SliderFloat("alpha", &settings_ref.itpl_alpha, 0, 1)) {
                    app.mesh.quadtree->UploadSettings();
                }
            }
            if (app.mesh.quadtree->capped) {
                ImGui::Text(" LOD FACTOR CAPPED \n");
            }
        }
    }  ImGui::End();

    ImGui::Render();
}

////////////////////////////////////////////////////////////////////////////////
///
/// Input Callbacks
///

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action,
                      int modsls)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case GLFW_KEY_R:
            app.mesh.quadtree->ReloadShaders();
            updateRenderParams();
            break;
        case GLFW_KEY_U:
            app.mesh.quadtree->ReconfigureShaders();
            break;
        case GLFW_KEY_P:
            app.cam.PrintStatus();
            break;
        default:
            break;
        }
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    if (GLFW_PRESS == action) {
        app.lbutton_down = (button == GLFW_MOUSE_BUTTON_LEFT);
        app.rbutton_down = (button == GLFW_MOUSE_BUTTON_RIGHT);
        glfwGetCursorPos(window, &app.x0, &app.y0);
        app.x0 -= app.gui_width;
    }  else if(GLFW_RELEASE == action) {
        app.rbutton_down = false;
        app.lbutton_down = false;
    }

}

void mouseMotionCallback(GLFWwindow* window, double x, double y)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;
    x = x - app.gui_width;

    if (app.lbutton_down || app.rbutton_down)
    {
        double dx, dy;
        dx = (x - app.x0) / app.cam.render_width;
        dy = (y - app.y0) / app.cam.render_height;

        if (app.lbutton_down)
            app.cam.ProcessMouseLeft(dx, dy);
        if (app.rbutton_down)
            app.cam.ProcessMouseRight(dx, dy);

        app.mesh.UpdateForView(app.cam);

        app.x0 = x;
        app.y0 = y;
    }
}

void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    app.cam.ProcessMouseScroll(yoffset);
    app.mesh.UpdateForView(app.cam);
}

void resizeCallback(GLFWwindow* window, int new_width, int new_height) {
    app.cam.render_width  = new_width - app.gui_width;
    app.cam.render_height = new_height;
    app.gui_height = new_height;
    app.mesh.quadtree->UpdateScreenRes(std::max(app.cam.render_height, app.cam.render_width));
    app.mesh.UpdateForSize(app.cam);
    app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
    app.mesh.quadtree->UploadSettings();
}

////////////////////////////////////////////////////////////////////////////////
///
/// The Program
///

void Init()
{
    cout << "******************************************************" << endl;
    cout << "INITIALIZATION" << endl;

    app.auto_lod = false;

    app.mode = TERRAIN;
    if(app.filepath != app.default_filepath)
        app.mode = MESH;

    app.cam.Init(app.mode);
    app.mesh.Init(app.mode, app.cam, app.filepath);
    bench.Init();
    updateRenderParams();

    app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
    app.mesh.quadtree->UploadSettings();

    cout << "END OF INITIALIZATION" << endl;
    cout << "******************************************************" << endl << endl;

}

void Draw()
{

    glViewport(app.gui_width, 0, app.cam.render_width, app.cam.render_height);
    app.mesh.Draw(bench.delta_T, app.mode);
    glViewport(0, 0, app.cam.render_width + app.gui_width, app.cam.render_height);
    bench.UpdateStats();
    RenderImgui();

    if (app.auto_lod && !app.mesh.quadtree->settings.uniform_on) {
        float& target = app.mesh.quadtree->settings.target_e_length;
        static float upperFPS = 75.0f, lowerFPS = 60.0f;
        if (bench.delta_T < 1.0/upperFPS) {
            target *= 0.99;
            app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
            app.mesh.quadtree->UploadSettings();
        } else if (bench.delta_T > 1.0/lowerFPS){
            target *= 1.01;
            app.mesh.quadtree->UpdateLodFactor(app.cam.render_width, app.cam.fov);
            app.mesh.quadtree->UploadSettings();
        }
    }
    bench.UpdateTime();
}

void Cleanup() {
    app.mesh.CleanUp();
}

void HandleArguments(int argc, char **argv)
{
    if (argc == 1) {
        app.filepath = app.default_filepath;
        cout << "Using default mesh: " << app.default_filepath << endl;
    } else {
        if (argc > 2)
            cout << "Only takes in 1 obj file name, ignoring other arguments" << endl;
        string file = argv[1];
        cout << "Trying to open " << file << " ... ";
        ifstream f(file.c_str());
        if (f.good()) {
            cout << "OK" << endl;
            app.filepath = file;
        } else {
            app.filepath = app.default_filepath;
            cout << "failure, keeping default mesh " << app.filepath << endl;
        }
    }
}

int main(int argc, char **argv)
{
    HandleArguments(argc, argv);

    app.cam.render_width = 1024;
    app.cam.render_height = 1024;
    app.gui_width = 352;
    app.gui_height = app.cam.render_height;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
    glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    // Create the Window
    LOG("Loading {Window-Main}\n");
    GLFWwindow* window = glfwCreateWindow((app.cam.render_width + app.gui_width), app.cam.render_height,
                                          "Distance Based Tessellation", NULL, NULL);
    if (window == NULL) {
        LOG("=> Failure <=\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, &keyboardCallback);
    glfwSetCursorPosCallback(window, &mouseMotionCallback);
    glfwSetMouseButtonCallback(window, &mouseButtonCallback);
    glfwSetScrollCallback(window, &mouseScrollCallback);
    glfwSetFramebufferSizeCallback(window, resizeCallback);

    // Load OpenGL functions
    LOG("Loading {OpenGL}\n");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("gladLoadGLLoader failed\n");
        return -1;
    }
#ifndef NDEBUG
    log_debug_output();
#endif

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    glDebugMessageCallback((GLDEBUGPROC)debug_output_logger, NULL);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    try {
        Init();
        ImGui::CreateContext();
        ImGui_ImplGlfwGL3_Init(window, false);
        ImGui::StyleColorsDark();

        // delta_T condition to avoid crashing my system
        while (!glfwWindowShouldClose(window)  && bench.delta_T < 5.0)
        {
            glfwPollEvents();
            ImGui_ImplGlfwGL3_NewFrame();
            Draw();
            ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);

        }
        ImGui_ImplGlfwGL3_Shutdown();
        ImGui::DestroyContext();
        Cleanup();
        glfwTerminate();

    } catch (std::exception& e) {
        LOG("%s", e.what());
        ImGui_ImplGlfwGL3_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
        LOG("(!) Demo Killed (!)\n");

        return EXIT_FAILURE;
    } catch (...) {
        ImGui_ImplGlfwGL3_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
        LOG("(!) Demo Killed (!)\n");

        return EXIT_FAILURE;
    }
    LOG("-- End -- Demo\n");

    return 0;
}
//////////////////////////////////////////////////////////////////////////////
