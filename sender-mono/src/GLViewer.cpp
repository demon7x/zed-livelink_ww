#include "GLViewer.hpp"
#include <random>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>

#include "imgui.h"
#include "imgui_impl_glut.h"
#include "imgui_impl_opengl3.h"

#if defined(_DEBUG) && defined(_WIN32)
#error "This sample should not be built in Debug mode, use RelWithDebInfo if you want to do step by step."
#endif

GLchar* VERTEX_SHADER =
"#version 330 core\n"
"layout(location = 0) in vec3 in_Vertex;\n"
"layout(location = 1) in vec4 in_Color;\n"
"uniform mat4 u_mvpMatrix;\n"
"out vec4 b_color;\n"
"void main() {\n"
"   b_color = in_Color;\n"
"	gl_Position = u_mvpMatrix * vec4(in_Vertex, 1);\n"
"}";

GLchar* FRAGMENT_SHADER =
"#version 330 core\n"
"in vec4 b_color;\n"
"layout(location = 0) out vec4 out_Color;\n"
"void main() {\n"
"   out_Color = b_color;\n"
"}";

// Textured full-screen quad shaders
GLchar* TEX_VERTEX_SHADER =
"#version 330 core\n"
"layout(location = 0) in vec3 in_vertex;\n"
"layout(location = 1) in vec2 in_texCoord;\n"
"out vec2 v_uv;\n"
"void main() {\n"
"   v_uv = in_texCoord;\n"
"   gl_Position = vec4(in_vertex, 1.0);\n"
"}";

GLchar* TEX_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec2 v_uv;\n"
"uniform sampler2D u_image;\n"
"layout(location = 0) out vec4 out_Color;\n"
"void main() {\n"
"   out_Color = texture(u_image, v_uv);\n"
"}";

GLchar* SK_VERTEX_SHADER =
"#version 330 core\n"
"layout(location = 0) in vec3 in_Vertex;\n"
"layout(location = 1) in vec4 in_Color;\n"
"layout(location = 2) in vec3 in_Normal;\n"
"out vec4 b_color;\n"
"out vec3 b_position;\n"
"out vec3 b_normal;\n"
"uniform mat4 u_mvpMatrix;\n"
"uniform vec4 u_color;\n"
"void main() {\n"
"   b_color = in_Color;\n"
"   b_position = in_Vertex;\n"
"   b_normal = in_Normal;\n"
"	gl_Position =  u_mvpMatrix * vec4(in_Vertex, 1);\n"
"}";

GLchar* SK_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec4 b_color;\n"
"in vec3 b_position;\n"
"in vec3 b_normal;\n"
"layout(location = 0) out vec4 out_Color;\n"
"void main() {\n"
"	vec3 lightPosition = vec3(3000, 5000, 0);\n"
"	vec3 ambient = 0.6f * vec3(0.95,0.98,0.99);\n"
"	vec3 lightDir = normalize(lightPosition - b_position);\n"
"	float diffuse = max(dot(b_normal, lightDir), 0.0);\n"
"   out_Color = vec4(b_color.rgb * (ambient + diffuse), 1.f);\n"
"}";

void addVert(Simple3DObject& obj, float i_f, float limit, float height, sl::float4& clr) {
    auto p1 = sl::float3(i_f, height, -limit);
    auto p2 = sl::float3(i_f, height, limit);
    auto p3 = sl::float3(-limit, height, i_f);
    auto p4 = sl::float3(limit, height, i_f);

    obj.addLine(p1, p2, clr);
    obj.addLine(p3, p4, clr);
}

GLViewer* currentInstance_ = nullptr;

float const colors[5][3] = {
    {.231f, .909f, .69f},
    {.098f, .686f, .816f},
    {.412f, .4f, .804f},
    {1, .725f, .0f},
    {.989f, .388f, .419f}
};

inline sl::float4 generateColorClass(int idx) {
    int const offset = std::max(0, idx % 5);
    return sl::float4(colors[offset][0], colors[offset][1], colors[offset][2], 1.f);
}

float const id_colors[8][3] = {
    { 232.0f, 176.0f, 59.0f},
    { 175.0f, 208.0f, 25.0f},
    { 102.0f, 205.0f, 105.0f},
    { 185.0f, 0.0f, 255.0f},
    { 99.0f, 107.0f, 252.0f},
    {252.0f, 225.0f, 8.0f},
    {167.0f, 130.0f, 141.0f},
    {194.0f, 72.0f, 113.0f}
};

inline sl::float4 generateColorID(int idx) {
    if (idx < 0) return sl::float4(236 / 255.0f, 184 / 255.0f, 36 / 255.0f, 255 / 255.0f);
    else {
        int const offset = std::max(0, idx % 8);
        return sl::float4(id_colors[offset][2] / 255.0f, id_colors[offset][1] / 255.0f, id_colors[offset][0] / 255.0f, 1.f);
    }
}

// Recognition test: the 13 joints that must be detected (head = NOSE on BODY_38).
const BODY_38_PARTS JUDGED_JOINTS[13] = {
    BODY_38_PARTS::NOSE,
    BODY_38_PARTS::LEFT_SHOULDER, BODY_38_PARTS::RIGHT_SHOULDER,
    BODY_38_PARTS::LEFT_ELBOW,    BODY_38_PARTS::RIGHT_ELBOW,
    BODY_38_PARTS::LEFT_WRIST,    BODY_38_PARTS::RIGHT_WRIST,
    BODY_38_PARTS::LEFT_HIP,      BODY_38_PARTS::RIGHT_HIP,
    BODY_38_PARTS::LEFT_KNEE,     BODY_38_PARTS::RIGHT_KNEE,
    BODY_38_PARTS::LEFT_ANKLE,    BODY_38_PARTS::RIGHT_ANKLE
};
const char* const JUDGED_JOINT_NAMES[13] = {
    "Head", "L Shoulder", "R Shoulder", "L Elbow", "R Elbow", "L Wrist", "R Wrist",
    "L Hip", "R Hip", "L Knee", "R Knee", "L Ankle", "R Ankle"
};

// The 10 motions of the recognition test, in test order. Korean names need a CJK font
// (loaded in initImGui); English is used when none is installed. English goes to the CSV.
struct TestMotion {
    const char* ko;
    const char* en;
};
const TestMotion TEST_MOTIONS[] = {
    {"정면 정자세",     "Front standing"},
    {"정면 걷기",       "Front walking"},
    {"측면 정자세",     "Side standing"},
    {"측면 걷기",       "Side walking"},
    {"양팔 들어올리기", "Raise both arms"},
    {"한 손 흔들기",    "Wave one hand"},
    {"한 다리 들기",    "Raise one leg"},
    {"앉았다 일어서기", "Sit down and stand up"},
    {"제자리 점프",     "Jump in place"},
    {"상체 좌우 회전",  "Twist upper body"},
};
const int NUM_TEST_MOTIONS = (int)(sizeof(TEST_MOTIONS) / sizeof(TEST_MOTIONS[0]));

static ImFont* korean_font = nullptr;          // nullptr when no CJK font was found
static ImVector<ImWchar> korean_glyph_ranges;  // must outlive the font atlas build

inline const char* motionName(int motion) {
    return korean_font ? TEST_MOTIONS[motion].ko : TEST_MOTIONS[motion].en;
}

inline bool isJudgedJoint(int kp_idx) {
    for (auto part : JUDGED_JOINTS)
        if (getIdx(part) == kp_idx) return true;
    return false;
}

// The SDK reports undetected 2D keypoints as non-finite or negative: a joint only counts as
// detected (and is only drawn) when it lands inside the image.
inline bool keypointInImage(const sl::float2& p, int w, int h) {
    return std::isfinite(p.x) && std::isfinite(p.y) &&
           p.x >= 0.f && p.y >= 0.f && p.x < (float)w && p.y < (float)h;
}

GLViewer::GLViewer() : available(false) {
    currentInstance_ = this;
    mouseButton_[0] = mouseButton_[1] = mouseButton_[2] = false;
    clearInputs();
    previousMouseMotion_[0] = previousMouseMotion_[1] = 0;
    draw_3d_skeletons_ = false;
    current_body_index_ = -1;
}

GLViewer::~GLViewer() {
}

void GLViewer::exit() {
    if (currentInstance_) {
        shutdownImGui();
        available = false;
    }
}

bool GLViewer::isAvailable() {
    glutMainLoopEvent();
    return available;
}

void CloseFunc(void) {
    if (currentInstance_) currentInstance_->exit();
}

void GLViewer::init(int argc, char** argv) {

    glutInit(&argc, argv);
    int wnd_w = glutGet(GLUT_SCREEN_WIDTH);
    int wnd_h = glutGet(GLUT_SCREEN_HEIGHT);
    
    glutInitWindowSize(wnd_w * 0.8, wnd_h * 0.8);
    glutInitWindowPosition(wnd_w * 0.1, wnd_h * 0.1);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);

    glutCreateWindow("WW Body Tracking");
    window_w_ = glutGet(GLUT_WINDOW_WIDTH);
    window_h_ = glutGet(GLUT_WINDOW_HEIGHT);

    GLenum err = glewInit();
    if (GLEW_OK != err)
        std::cout << "ERROR: glewInit failed: " << glewGetErrorString(err) << "\n";

    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Compile and create the shader for 3D objects
    shaderSK.it = Shader(SK_VERTEX_SHADER, SK_FRAGMENT_SHADER);
    shaderSK.MVP_Mat = glGetUniformLocation(shaderSK.it.getProgramId(), "u_mvpMatrix");

    shaderLine.it = Shader(VERTEX_SHADER, FRAGMENT_SHADER);
    shaderLine.MVP_Mat = glGetUniformLocation(shaderLine.it.getProgramId(), "u_mvpMatrix");

    // Shader to render camera image as background
    shaderTex.it = Shader(TEX_VERTEX_SHADER, TEX_FRAGMENT_SHADER);

    // Create full-screen quad geometry (NDC)
    {
        GLfloat quadVertices[12] = {
            -1.0f, -1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f, 0.0f
        };
        GLfloat quadUVs[8] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f
        };
        glGenVertexArrays(1, &image_vao_);
        glBindVertexArray(image_vao_);
        glGenBuffers(2, image_vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, image_vbo_[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, image_vbo_[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadUVs), quadUVs, GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glGenTextures(1, &image_tex_id_);
        glBindTexture(GL_TEXTURE_2D, image_tex_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Create the viewer camera — placed 2m above floor, 4m back, looking at origin.
    // (Floor grid sits at y=0 since set_floor_as_origin=true; viewer needs to be off-plane.)
    camera_ = CameraGL(sl::Translation(0.f, 2000.f, 4000.f),
                       sl::Translation(0.f, -2000.f, -4000.f));
    //camera_.setOffsetFromPosition(sl::Translation(0, 0, 1000));

    // Create the skeletons objects
    skeletons = Simple3DObject(sl::Translation(0, 0, 0), false);
    skeletons.setDrawingType(GL_QUADS);

    // Set background color (black)
    bckgrnd_clr = sl::float4(0.2f, 0.19f, 0.2f, 1.0f);

    floor_grid = Simple3DObject(sl::Translation(0, 0, 0), false);
    floor_grid.setDrawingType(GL_LINES);

    // 2D overlay container (drawn in screen space)
    overlay2d = Simple3DObject(sl::Translation(0, 0, 0), false);
    overlay2d.setDrawingType(GL_LINES);

    // Tracking-region rectangle outline (rebuilt lazily when edges change).
    region_outline_ = Simple3DObject(sl::Translation(0, 0, 0), false);
    region_outline_.setDrawingType(GL_LINES);

    // Floor grid: rectangle 8m wide (X) x 12m deep (Z), centered on world origin,
    // 50cm spacing. Sits at y=0 (ZED-detected floor plane).
    const float half_x_m = 4.0f;                  // total width  = 2 * 4m = 8m
    const float half_z_m = 6.0f;                  // total depth  = 2 * 6m = 12m
    const float step_m   = 0.5f;
    const float grid_height_mm = 0.f;
    sl::float4 clr_grid(255, 255, 255, 255);
    clr_grid /= 255.f;

    int nx = (int)(half_x_m / step_m);
    for (int i = -nx; i <= nx; ++i) {
        float x_mm = i * step_m * 1000.f;
        floor_grid.addLine(
            sl::float3(x_mm, grid_height_mm, -half_z_m * 1000.f),
            sl::float3(x_mm, grid_height_mm,  half_z_m * 1000.f),
            clr_grid);
    }
    int nz = (int)(half_z_m / step_m);
    for (int i = -nz; i <= nz; ++i) {
        float z_mm = i * step_m * 1000.f;
        floor_grid.addLine(
            sl::float3(-half_x_m * 1000.f, grid_height_mm, z_mm),
            sl::float3( half_x_m * 1000.f, grid_height_mm, z_mm),
            clr_grid);
    }

    floor_grid.pushToGPU();

    glDisable(GL_DEPTH_TEST);

    // Initialize Dear ImGui (creates context, hooks GLUT input, sets up GL3 renderer).
    // Must run before our own glut*Func() calls so ours overwrite ImGui's and delegate to it.
    initImGui();

    // Map glut function on this class methods
    glutDisplayFunc(GLViewer::drawCallback);
    glutMouseFunc(GLViewer::mouseButtonCallback);
    glutMotionFunc(GLViewer::mouseMotionCallback);
    glutReshapeFunc(GLViewer::reshapeCallback);
    glutKeyboardFunc(GLViewer::keyPressedCallback);
    glutKeyboardUpFunc(GLViewer::keyReleasedCallback);
    glutCloseFunc(CloseFunc);
    available = true;
}

void GLViewer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't write imgui.ini next to the binary
    ImGui::StyleColorsDark();

    // ImGui's built-in font is ASCII only: take the Hangul glyphs of the test motion names
    // from a system CJK font when one is installed.
    io.Fonts->AddFontDefault();
    const char* korean_font_paths[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
        "C:/Windows/Fonts/malgun.ttf",
    };
    for (const char* path : korean_font_paths) {
        FILE* f = std::fopen(path, "rb");
        if (!f) continue;
        std::fclose(f);
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        for (const TestMotion& m : TEST_MOTIONS) builder.AddText(m.ko);
        builder.BuildRanges(&korean_glyph_ranges);
        korean_font = io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr, korean_glyph_ranges.Data);
        if (korean_font) break;
    }

    ImGui_ImplGLUT_Init();
    ImGui_ImplGLUT_InstallFuncs();
    ImGui_ImplOpenGL3_Init("#version 330");
    imgui_initialized_ = true;
}

void GLViewer::shutdownImGui() {
    if (!imgui_initialized_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGLUT_Shutdown();
    ImGui::DestroyContext();
    imgui_initialized_ = false;
}

void GLViewer::render() {
    if (available) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(bckgrnd_clr.r, bckgrnd_clr.g, bckgrnd_clr.b, 1.f);
        update();
        draw();

        if (imgui_initialized_) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGLUT_NewFrame();
            ImGui::NewFrame();
            if (!clean_view_) drawImGuiPanel();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        glutSwapBuffers();
        glutPostRedisplay();
    }
}

inline bool renderObject(const sl::BodyData& i, const bool isTrackingON) {
    if (isTrackingON)
        return (i.tracking_state == sl::OBJECT_TRACKING_STATE::OK);
    else
        return (i.tracking_state == sl::OBJECT_TRACKING_STATE::OK || i.tracking_state == sl::OBJECT_TRACKING_STATE::OFF);
}

template<typename T>
void createSKPrimitive(sl::BodyData& body, const std::vector<std::pair<T, T>>& map, Simple3DObject& skp, sl::float4 clr_id) {
    for (auto& limb : map) {
        sl::float3 kp_1 = body.keypoint[getIdx(limb.first)];
        sl::float3 kp_2 = body.keypoint[getIdx(limb.second)];
        float norm_1 = kp_1.norm();
        float norm_2 = kp_2.norm();
        // draw cylinder between two keypoints
        if (std::isfinite(norm_1) && std::isfinite(norm_2))
            skp.addCylinder(kp_1, kp_2, clr_id);
    }
    for (int j = 0; j < static_cast<int> (T::LAST); j++) {
        sl::float3 kp = body.keypoint[j];
        if (std::isfinite(kp.norm()))
            skp.addSphere(kp, clr_id, 0.01f * 1000.0f);
    }
}

void createSKPrimitivePtOnly(sl::BodyData& body, const int idx_start, const int idx_end, Simple3DObject& skp, sl::float4 clr_id) {
    for (int j = idx_start; j <= idx_end; j++) {
        sl::float3 kp = body.keypoint[j];
        if (std::isfinite(kp.norm()))
            skp.addSphere(kp, clr_id, 0.01f * 1000.0f);
    }
}

void GLViewer::updateData(Bodies& bodies, sl::Transform& pose) {
    mtx.lock();
    skeletons.clear();
    overlay2d.clear();
    current_body_ids_.clear();
    body_label_anchor_ndc_.clear();
    body_in_region_.clear();
    selected_ids_.clear();   // rebuilt from region + manual overrides each frame
    cam_pose = pose;
    sl::float3 tr_0(0, 0, 0);
    cam_pose.setTranslation(tr_0);

    for (auto& it : bodies.body_list) {
        if (renderObject(it, bodies.is_tracked)) {
            current_body_ids_.push_back(it.id);

            // Region-gated auto-selection. body.position is in WORLD frame (mm) because
            // main.cpp sets rt_params.measure3D_reference_frame = REFERENCE_FRAME::WORLD.
            float bx_m = it.position.x / 1000.f;
            float bz_m = it.position.z / 1000.f;
            bool in_region =
                bx_m >= region_x_min_m_ && bx_m <= region_x_max_m_ &&
                bz_m >= region_z_min_m_ && bz_m <= region_z_max_m_;
            body_in_region_[it.id] = in_region;

            auto it_override = manual_overrides_.find(it.id);
            bool effective = (it_override != manual_overrides_.end())
                                ? it_override->second
                                : in_region;
            if (effective) selected_ids_.insert(it.id);

            // draw skeletons
            auto clr_id = generateColorID(it.id);
            if (draw_3d_skeletons_) {
                if (it.keypoint.size() == 18)
                    createSKPrimitive(it, BODY_18_BONES, skeletons, clr_id);
                else if (it.keypoint.size() == 34) {
                    createSKPrimitive(it, BODY_34_BONES, skeletons, clr_id);
                }
                else {
                    createSKPrimitive(it, BODY_BONES_FAST_RENDER, skeletons, clr_id);
                    // Draw rest of the body with kp only, and smaller (for high link density parts like hands)
                    createSKPrimitivePtOnly(it, getIdx(BODY_38_PARTS::RIGHT_WRIST) + 1, it.keypoint.size() - 1, skeletons, clr_id);
                }
            }

            // Build 2D overlay in screen space (precise image overlay)
            if (image_w_ > 0 && image_h_ > 0 && it.keypoint_2d.size() > 0) {
                auto to_ndc = [&](float px, float py) -> sl::float3 {
                    float x = (px / (float)image_w_) * 2.f - 1.f;
                    float y = 1.f - (py / (float)image_h_) * 2.f;
                    return sl::float3(x, y, 0.0f);
                };
                bool selected_mode = !selected_ids_.empty();
                bool is_selected = !selected_mode || (selected_ids_.find(it.id) != selected_ids_.end());
                sl::float3 clr2d = is_selected ? sl::float3(1.0f, 1.0f, 0.0f) : sl::float3(0.4f, 0.4f, 0.4f);

                // Draw bones as lines in 2D
                for (auto& limb : BODY_BONES_FAST_RENDER) {
                    int i0 = getIdx(limb.first);
                    int i1 = getIdx(limb.second);
                    if (i0 < it.keypoint_2d.size() && i1 < it.keypoint_2d.size()) {
                        auto p0 = it.keypoint_2d[i0];
                        auto p1 = it.keypoint_2d[i1];
                        if (keypointInImage(p0, image_w_, image_h_) && keypointInImage(p1, image_w_, image_h_)) {
                            overlay2d.addLine(to_ndc(p0.x, p0.y), to_ndc(p1.x, p1.y), clr2d);
                        }
                    }
                }
                // Draw joint markers as small crosses (larger for the 13 judged joints)
                for (int j = 0; j < (int)it.keypoint_2d.size(); ++j) {
                    auto p = it.keypoint_2d[j];
                    if (!keypointInImage(p, image_w_, image_h_)) continue;
                    const float s_px = isJudgedJoint(j) ? 7.0f : 3.0f;
                    const float dx = (s_px * 2.f) / (float)image_w_;
                    const float dy = (s_px * 2.f) / (float)image_h_;
                    float x_ndc = (p.x / (float)image_w_) * 2.f - 1.f;
                    float y_ndc = 1.f - (p.y / (float)image_h_) * 2.f;
                    overlay2d.addLine(sl::float3(x_ndc - dx, y_ndc, 0.f), sl::float3(x_ndc + dx, y_ndc, 0.f), clr2d);
                    overlay2d.addLine(sl::float3(x_ndc, y_ndc - dy, 0.f), sl::float3(x_ndc, y_ndc + dy, 0.f), clr2d);
                }

                // Cache an anchor (head/nose if available, else pelvis kp[0]) for label drawing
                auto pick_anchor = [&]() -> sl::float2 {
                    // Try head/nose-ish first (kp index 0 is pelvis on BODY_38; nose is index 5).
                    // Just use the first finite keypoint with smallest y (highest on screen).
                    sl::float2 best{std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::quiet_NaN()};
                    for (auto& p : it.keypoint_2d) {
                        if (!keypointInImage(p, image_w_, image_h_)) continue;
                        if (!std::isfinite(best.y) || p.y < best.y) best = p;
                    }
                    return best;
                };
                sl::float2 anchor = pick_anchor();
                if (std::isfinite(anchor.x) && std::isfinite(anchor.y)) {
                    float ax = (anchor.x / (float)image_w_) * 2.f - 1.f;
                    float ay = 1.f - (anchor.y / (float)image_h_) * 2.f;
                    body_label_anchor_ndc_[it.id] = std::make_pair(ax, ay);
                }
            }
        }
    }
    if (current_body_ids_.empty()) current_body_index_ = -1;
    else {
        if (current_body_index_ < 0) current_body_index_ = 0;
        if (current_body_index_ >= (int)current_body_ids_.size()) current_body_index_ = (int)current_body_ids_.size() - 1;
    }

    // ---- Single output slot ----
    // Free the slot if its occupant is no longer selected (left region / gone / overridden out).
    // A body whose tracking is only briefly lost (SEARCHING, e.g. occluded during a side pose)
    // keeps the slot for slot_grace_s_ so it doesn't jump to another body; nothing is streamed
    // meanwhile because main.cpp only sends bodies in OK state.
    if (slot_occupant_body_id_ != -1) {
        bool keep = selected_ids_.count(slot_occupant_body_id_) > 0;
        if (keep) {
            slot_occupant_lost_ = false;
        } else {
            bool searching = false;
            for (auto& it : bodies.body_list) {
                if (it.id == slot_occupant_body_id_ &&
                    it.tracking_state == sl::OBJECT_TRACKING_STATE::SEARCHING) searching = true;
            }
            auto it_override = manual_overrides_.find(slot_occupant_body_id_);
            bool forced_out = it_override != manual_overrides_.end() && !it_override->second;
            if (searching && !forced_out) {
                auto now = std::chrono::steady_clock::now();
                if (!slot_occupant_lost_) {
                    slot_occupant_lost_ = true;
                    slot_lost_since_ = now;
                }
                keep = std::chrono::duration<float>(now - slot_lost_since_).count() < slot_grace_s_;
            }
        }
        if (!keep) releaseSlot();
    }
    // Auto-assign: lowest body id in selected_ids_ claims the empty slot.
    if (slot_occupant_body_id_ == -1 && !selected_ids_.empty()) {
        int min_id = -1;
        for (int sid : selected_ids_) {
            if (min_id == -1 || sid < min_id) min_id = sid;
        }
        slot_occupant_body_id_ = min_id;
    }

    evaluateRecognition(bodies);
    mtx.unlock();
}

void GLViewer::updateImage(sl::Mat& image) {
    mtx.lock();
    int w = image.getWidth();
    int h = image.getHeight();
    glBindTexture(GL_TEXTURE_2D, image_tex_id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (w != image_w_ || h != image_h_) {
        image_w_ = w;
        image_h_ = h;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_w_, image_h_, 0, GL_BGRA, GL_UNSIGNED_BYTE, image.getPtr<sl::uchar1>(sl::MEM::CPU));
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_w_, image_h_, GL_BGRA, GL_UNSIGNED_BYTE, image.getPtr<sl::uchar1>(sl::MEM::CPU));
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    has_image_ = true;
    mtx.unlock();
}

void GLViewer::update() {
    if (keyStates_['q'] == KEY_STATE::UP || keyStates_['Q'] == KEY_STATE::UP || keyStates_[27] == KEY_STATE::UP) {
        currentInstance_->exit();
        return;
    }

    if (keyStates_['r'] == KEY_STATE::UP || keyStates_['R'] == KEY_STATE::UP) {
        if (zed_camera_set_) {
            applyZEDCameraPose();
        } else {
            camera_.setPosition(sl::Translation(0.0f, 2000.0f, 4000.0f));
            camera_.setDirection(sl::Translation(0.0f, -2000.0f, -4000.0f), sl::Translation(0.0f, 1.0f, 0.0f));
        }
    }

    if (keyStates_['t'] == KEY_STATE::UP || keyStates_['T'] == KEY_STATE::UP) {
        camera_.setPosition(sl::Translation(0.0f, 0.0f, 1500.0f));
        camera_.setOffsetFromPosition(sl::Translation(0.0f, 0.0f, 6000.0f));
        camera_.translate(sl::Translation(0.0f, 1500.0f, -4000.0f));
        camera_.setDirection(sl::Translation(0.0f, -1.0f, 0.0f), sl::Translation(0.0f, 1.0f, 0.0f));
    }

    // Rotate camera with mouse
    if (mouseButton_[MOUSE_BUTTON::LEFT]) {
        camera_.rotate(sl::Rotation((float)mouseMotion_[1] * MOUSE_R_SENSITIVITY, camera_.getRight()));
        camera_.rotate(sl::Rotation((float)mouseMotion_[0] * MOUSE_R_SENSITIVITY, camera_.getVertical() * -1.f));
    }

    // Translate camera with mouse
    if (mouseButton_[MOUSE_BUTTON::RIGHT]) {
        camera_.translate(camera_.getUp() * (float)mouseMotion_[1] * MOUSE_T_SENSITIVITY);
        camera_.translate(camera_.getRight() * (float)mouseMotion_[0] * MOUSE_T_SENSITIVITY);
    }

    // Zoom in with mouse wheel
    if (mouseWheelPosition_ != 0) {
        //float distance = sl::Translation(camera_.getOffsetFromPosition()).norm();
        if (mouseWheelPosition_ > 0 /* && distance > camera_.getZNear()*/) { // zoom
            camera_.translate(camera_.getForward() * MOUSE_UZ_SENSITIVITY * 500 * -1);
        }
        else if (/*distance < camera_.getZFar()*/ mouseWheelPosition_ < 0) {// unzoom
            //camera_.setOffsetFromPosition(camera_.getOffsetFromPosition() * MOUSE_DZ_SENSITIVITY);
            camera_.translate(camera_.getForward() * MOUSE_UZ_SENSITIVITY * 500);
        }
    }

    camera_.update();
    mtx.lock();
    // Update point cloud buffers
    if (draw_3d_skeletons_) skeletons.pushToGPU();
    overlay2d.pushToGPU();
    mtx.unlock();
    clearInputs();
}

void GLViewer::draw() {
    // Image, grid and overlays share a viewport letterboxed to the camera aspect ratio,
    // so the image is never stretched when the window is resized.
    updateImageViewport();
    glViewport(view_x_, view_y_, view_w_, view_h_);

    // Draw camera image as background
    if (has_image_ && image_tex_id_ != 0 && image_vao_ != 0) {
        glDisable(GL_DEPTH_TEST);
        glUseProgram(shaderTex.it.getProgramId());
        GLint texLoc = glGetUniformLocation(shaderTex.it.getProgramId(), "u_image");
        if (texLoc >= 0) glUniform1i(texLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, image_tex_id_);
        glBindVertexArray(image_vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

    // Clean view: camera image only.
    if (clean_view_) return;

    sl::Transform vpMatrix = camera_.getViewProjectionMatrix();
    glUseProgram(shaderLine.it.getProgramId());
    glUniformMatrix4fv(shaderLine.MVP_Mat, 1, GL_TRUE, vpMatrix.m);
    glLineWidth(2.f);
    floor_grid.draw();
    // Tracking region rectangle outline, on top of the grid, same projection.
    if (region_outline_dirty_) rebuildRegionOutline();
    glLineWidth(3.f);
    region_outline_.draw();
    glLineWidth(1.f);
    glUseProgram(0);

    // Apply IMU Rotation compensation
    vpMatrix = vpMatrix * cam_pose;

    if (draw_3d_skeletons_) {
        glEnable(GL_DEPTH_TEST);
        glUseProgram(shaderSK.it.getProgramId());
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glUniformMatrix4fv(shaderSK.MVP_Mat, 1, GL_TRUE, vpMatrix.m);
        skeletons.draw();
        glUseProgram(0);
        glDisable(GL_DEPTH_TEST);
    }

    // Draw 2D overlay (precise image-space overlay) on top
    if (overlay2d.getPosition().x == overlay2d.getPosition().x) { // always true; keep structure
        sl::Transform id; id.setIdentity();
        glUseProgram(shaderLine.it.getProgramId());
        glUniformMatrix4fv(shaderLine.MVP_Mat, 1, GL_TRUE, id.m);
        glLineWidth(2.f);
        overlay2d.draw();
        glUseProgram(0);
    }
}

// Selection API
std::vector<int> GLViewer::getSelectedIds() const {
    return std::vector<int>(selected_ids_.begin(), selected_ids_.end());
}

std::string GLViewer::getLabel(int body_id) const {
    auto it = body_labels_.find(body_id);
    return (it == body_labels_.end()) ? std::string() : it->second;
}

void GLViewer::setZEDCameraPose(const sl::Transform& pose, float hfov_deg, float vfov_deg) {
    zed_camera_pose_ = pose;
    zed_hfov_ = hfov_deg;
    zed_vfov_ = vfov_deg;
    zed_camera_set_ = true;
    applyZEDCameraPose();
}

void GLViewer::rebuildRegionOutline() {
    region_outline_.clear();
    const float y = 0.f;
    const float x0 = region_x_min_m_ * 1000.f;
    const float x1 = region_x_max_m_ * 1000.f;
    const float z0 = region_z_min_m_ * 1000.f;
    const float z1 = region_z_max_m_ * 1000.f;
    sl::float4 clr(255.f, 200.f, 0.f, 255.f);   // yellow
    clr /= 255.f;
    region_outline_.addLine(sl::float3(x0, y, z0), sl::float3(x1, y, z0), clr);
    region_outline_.addLine(sl::float3(x1, y, z0), sl::float3(x1, y, z1), clr);
    region_outline_.addLine(sl::float3(x1, y, z1), sl::float3(x0, y, z1), clr);
    region_outline_.addLine(sl::float3(x0, y, z1), sl::float3(x0, y, z0), clr);
    region_outline_.pushToGPU();
    region_outline_dirty_ = false;
}

void GLViewer::applyZEDCameraPose() {
    if (!zed_camera_set_) return;
    // setDirection(from->to) inside CameraGL goes through a buggy
    // sl::Translation*sl::Orientation path that produces a non-unit basis;
    // copy ZED's orientation straight into rotation_ via setRotation instead.
    camera_.setPosition(zed_camera_pose_.getTranslation());
    camera_.setRotation(zed_camera_pose_.getOrientation());
    camera_.setProjection(zed_hfov_, zed_vfov_, 200.f, 200000.f);
}

void GLViewer::setSelectionDefaults(float x_min_m, float x_max_m, float z_min_m, float z_max_m,
                                    int subject_id, float slot_grace_s) {
    region_default_x_min_m_ = region_x_min_m_ = x_min_m;
    region_default_x_max_m_ = region_x_max_m_ = x_max_m;
    region_default_z_min_m_ = region_z_min_m_ = z_min_m;
    region_default_z_max_m_ = region_z_max_m_ = z_max_m;
    region_outline_dirty_ = true;
    fixed_subject_id_ = subject_id;
    slot_grace_s_ = slot_grace_s;
}

void GLViewer::updateImageViewport() {
    view_x_ = 0;
    view_y_ = 0;
    view_w_ = window_w_;
    view_h_ = window_h_;
    if (image_w_ <= 0 || image_h_ <= 0 || window_w_ <= 0 || window_h_ <= 0) return;
    float image_aspect = (float)image_w_ / (float)image_h_;
    if ((float)window_w_ / (float)window_h_ > image_aspect) {
        // Window wider than the image: bars left and right.
        view_w_ = (int)(window_h_ * image_aspect + 0.5f);
        view_x_ = (window_w_ - view_w_) / 2;
    } else {
        // Window taller than the image: bars top and bottom.
        view_h_ = (int)(window_w_ / image_aspect + 0.5f);
        view_y_ = (window_h_ - view_h_) / 2;
    }
}

void GLViewer::evaluateRecognition(Bodies& bodies) {
    // Live: which of the 13 judged joints the slot body shows in this frame.
    slot_joints_detected_ = -1;
    std::fill(std::begin(slot_joint_detected_), std::end(slot_joint_detected_), false);
    for (auto& it : bodies.body_list) {
        if (it.id != slot_occupant_body_id_ || it.tracking_state != sl::OBJECT_TRACKING_STATE::OK) continue;
        if (it.keypoint_2d.size() != static_cast<size_t>(BODY_38_PARTS::LAST)) break; // judged joints are BODY_38
        slot_joints_detected_ = 0;
        for (int k = 0; k < 13; ++k) {
            slot_joint_detected_[k] = keypointInImage(it.keypoint_2d[getIdx(JUDGED_JOINTS[k])], image_w_, image_h_);
            if (slot_joint_detected_[k]) ++slot_joints_detected_;
        }
        break;
    }

    if (!trial_running_) return;
    auto now = std::chrono::steady_clock::now();
    if (bodies.is_new) {
        // One sample per new body tracking result; a result without the slot body counts as missing.
        trial_current_.frames++;
        if (slot_joints_detected_ == 13) {
            trial_current_.frames_all_joints++;
            if (trial_gap_open_) {
                trial_current_.longest_gap_s = std::max(trial_current_.longest_gap_s,
                    std::chrono::duration<float>(now - trial_gap_start_).count());
                trial_gap_open_ = false;
            }
        } else if (!trial_gap_open_) {
            trial_gap_open_ = true;
            trial_gap_start_ = now;
        }
    }
    if (std::chrono::duration<float>(now - trial_start_).count() >= trial_duration_s_) finishTrial(now);
}

void GLViewer::startTrial() {
    trial_current_ = RecognitionTrial();
    trial_current_.motion = trial_motion_;
    trial_gap_open_ = false;
    trial_start_ = std::chrono::steady_clock::now();
    trial_running_ = true;
}

void GLViewer::finishTrial(std::chrono::steady_clock::time_point now) {
    if (trial_gap_open_) {
        trial_current_.longest_gap_s = std::max(trial_current_.longest_gap_s,
            std::chrono::duration<float>(now - trial_gap_start_).count());
        trial_gap_open_ = false;
    }
    float ratio = trial_current_.frames > 0
                ? (float)trial_current_.frames_all_joints / (float)trial_current_.frames : 0.f;
    trial_current_.success = trial_current_.frames > 0 && ratio >= trial_pass_ratio_;
    trials_.push_back(trial_current_);
    trial_running_ = false;
    trial_motion_ = (trial_current_.motion + 1) % NUM_TEST_MOTIONS; // next motion in test order

    // Evidence: one CSV row per trial, with the recognition rate so far, in the working directory.
    int successes = (int)std::count_if(trials_.begin(), trials_.end(),
                                       [](const RecognitionTrial& t) { return t.success; });
    bool new_file = trial_csv_path_.empty();
    if (new_file) {
        char stamp[32];
        std::time_t t = std::time(nullptr);
        std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", std::localtime(&t));
        trial_csv_path_ = std::string("recognition_") + stamp + ".csv";
    }
    std::ofstream csv(trial_csv_path_, std::ios::app);
    if (new_file)
        csv << "trial,motion_no,motion,duration_s,results,results_13_joints,ratio_pct,longest_gap_s,result,"
               "successes,trials,recognition_rate_pct\n";
    csv << std::fixed << std::setprecision(2)
        << trials_.size() << ',' << trial_current_.motion + 1 << ',' << TEST_MOTIONS[trial_current_.motion].en << ','
        << trial_duration_s_ << ',' << trial_current_.frames << ',' << trial_current_.frames_all_joints << ','
        << ratio * 100.f << ',' << trial_current_.longest_gap_s << ',' << (trial_current_.success ? "OK" : "FAIL") << ','
        << successes << ',' << trials_.size() << ',' << 100.f * successes / (float)trials_.size() << '\n';
}

void GLViewer::drawRecognitionPanel() {
    ImGui::SeparatorText("Recognition test (13 joints)");

    // Live: which judged joints the slot body shows right now.
    if (slot_joints_detected_ < 0) {
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.5f, 1.f), "Slot body: none");
    } else {
        ImGui::TextColored(slot_joints_detected_ == 13 ? ImVec4(0.2f, 1.0f, 0.2f, 1.f) : ImVec4(1.0f, 0.4f, 0.4f, 1.f),
                           "Slot body %d: %d / 13 joints detected", slot_occupant_body_id_, slot_joints_detected_);
    }
    if (ImGui::BeginTable("##joints", 4, ImGuiTableFlags_SizingStretchSame)) {
        for (int k = 0; k < 13; ++k) {
            ImGui::TableNextColumn();
            bool detected = slot_joints_detected_ >= 0 && slot_joint_detected_[k];
            ImGui::TextColored(detected ? ImVec4(0.2f, 1.0f, 0.2f, 1.f) : ImVec4(0.5f, 0.5f, 0.5f, 1.f),
                               "%s", JUDGED_JOINT_NAMES[k]);
        }
        ImGui::EndTable();
    }

    // Timed trials, one per motion: success when all 13 joints show in at least
    // trial_pass_ratio_ of the body tracking results of the trial.
    ImGui::BeginDisabled(trial_running_);
    char label[64];
    if (korean_font) ImGui::PushFont(korean_font);
    std::snprintf(label, sizeof(label), "%d. %s", trial_motion_ + 1, motionName(trial_motion_));
    ImGui::SetNextItemWidth(240.f);
    if (ImGui::BeginCombo("Motion", label)) {
        for (int m = 0; m < NUM_TEST_MOTIONS; ++m) {
            std::snprintf(label, sizeof(label), "%d. %s", m + 1, motionName(m));
            if (ImGui::Selectable(label, m == trial_motion_)) trial_motion_ = m;
        }
        ImGui::EndCombo();
    }
    if (korean_font) ImGui::PopFont();
    ImGui::SetNextItemWidth(240.f);
    ImGui::SliderFloat("Duration (s)", &trial_duration_s_, 1.f, 10.f, "%.1f");
    float pass_pct = trial_pass_ratio_ * 100.f;
    ImGui::SetNextItemWidth(240.f);
    if (ImGui::SliderFloat("Pass (% of results)", &pass_pct, 50.f, 100.f, "%.0f%%"))
        trial_pass_ratio_ = pass_pct / 100.f;
    ImGui::EndDisabled();

    if (trial_running_) {
        float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - trial_start_).count();
        ImGui::ProgressBar(std::min(elapsed / trial_duration_s_, 1.f), ImVec2(240.f, 0.f), "recording...");
    } else if (ImGui::Button("Start trial")) {
        startTrial();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(trial_running_ || trials_.empty());
    if (ImGui::Button("Clear trials")) {
        trials_.clear();
        trial_motion_ = 0;
        trial_csv_path_.clear(); // the next trial starts a new CSV
    }
    ImGui::EndDisabled();

    if (trials_.empty()) return;
    if (ImGui::BeginTable("##trials", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Motion");
        ImGui::TableSetupColumn("13 joints");
        ImGui::TableSetupColumn("Longest gap");
        ImGui::TableSetupColumn("Result");
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < trials_.size(); ++i) {
            const RecognitionTrial& t = trials_[i];
            float ratio = t.frames > 0 ? (float)t.frames_all_joints / (float)t.frames : 0.f;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", (int)i + 1);
            ImGui::TableNextColumn();
            if (korean_font) ImGui::PushFont(korean_font);
            ImGui::TextUnformatted(motionName(t.motion));
            if (korean_font) ImGui::PopFont();
            ImGui::TableNextColumn();
            ImGui::Text("%.0f%%", ratio * 100.f);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f s", t.longest_gap_s);
            ImGui::TableNextColumn();
            ImGui::TextColored(t.success ? ImVec4(0.2f, 1.0f, 0.2f, 1.f) : ImVec4(1.0f, 0.4f, 0.4f, 1.f),
                               t.success ? "OK" : "FAIL");
        }
        ImGui::EndTable();
    }
    int successes = (int)std::count_if(trials_.begin(), trials_.end(),
                                       [](const RecognitionTrial& t) { return t.success; });
    ImGui::Text("Recognized %d / %d  ->  recognition rate %.1f%%", successes, (int)trials_.size(),
                100.f * successes / (float)trials_.size());
    ImGui::TextDisabled("CSV: %s", trial_csv_path_.c_str());
}

void GLViewer::drawImGuiPanel() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 760), ImGuiCond_FirstUseEver);
    ImGui::Begin("ZED Subjects");

    // ---- Tracking region (world meters, y=0 plane) ----
    ImGui::SeparatorText("Tracking region (m)");
    bool moved = false;
    moved |= ImGui::SliderFloat("X min", &region_x_min_m_, -10.f, 10.f, "%.2f");
    moved |= ImGui::SliderFloat("X max", &region_x_max_m_, -10.f, 10.f, "%.2f");
    moved |= ImGui::SliderFloat("Z min", &region_z_min_m_, -10.f, 10.f, "%.2f");
    moved |= ImGui::SliderFloat("Z max", &region_z_max_m_, -10.f, 10.f, "%.2f");
    if (moved) region_outline_dirty_ = true;
    if (ImGui::Button("Reset region")) {
        region_x_min_m_ = region_default_x_min_m_; region_x_max_m_ = region_default_x_max_m_;
        region_z_min_m_ = region_default_z_min_m_; region_z_max_m_ = region_default_z_max_m_;
        region_outline_dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear overrides")) manual_overrides_.clear();

    // ---- Output slot ----
    ImGui::SeparatorText("Output slot (single subject)");
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputInt("Subject id (UE)", &fixed_subject_id_);
    if (fixed_subject_id_ < 0) fixed_subject_id_ = 0;
    if (slot_occupant_body_id_ == -1) {
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.5f, 1.f),
                           "Slot: (empty) -> no data sent");
    } else {
        ImGui::Text("Slot: body %d  ->  UE subject %d",
                    slot_occupant_body_id_, fixed_subject_id_);
        ImGui::SameLine();
        if (ImGui::SmallButton("Release")) releaseSlot();
        if (slot_occupant_lost_) {
            float held = std::chrono::duration<float>(std::chrono::steady_clock::now() - slot_lost_since_).count();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Tracking lost: holding slot %.1f / %.1f s (not sent)",
                               held, slot_grace_s_);
        }
    }
    ImGui::SetNextItemWidth(120.f);
    ImGui::SliderFloat("Slot grace (s)", &slot_grace_s_, 0.f, 3.f, "%.1f");

    ImGui::Separator();
    ImGui::Text("Detected: %zu  |  Selected: %zu",
                current_body_ids_.size(), selected_ids_.size());
    if (selected_ids_.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "No subject selected -> no data sent");

    if (ImGui::Button("Select All")) selectAllCurrent();
    ImGui::SameLine();
    if (ImGui::Button("None")) selectNone();
    ImGui::Separator();

    // ---- Bodies currently visible ----
    if (current_body_ids_.empty()) {
        ImGui::TextDisabled("(no bodies in frame)");
    } else {
        for (int id : current_body_ids_) {
            ImGui::PushID(id);
            bool in_region = body_in_region_.count(id) ? body_in_region_[id] : false;
            bool overridden = manual_overrides_.count(id) > 0;
            bool effective  = selected_ids_.count(id) > 0;

            // IN/OUT badge (green = in region, gray = outside)
            ImGui::TextColored(in_region ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
                                         : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               in_region ? "IN " : "OUT");
            ImGui::SameLine();
            if (ImGui::Checkbox("##sel", &effective)) {
                manual_overrides_[id] = effective;  // user choice sticks
                if (effective) selected_ids_.insert(id);
                else           selected_ids_.erase(id);
            }
            ImGui::SameLine();
            ImGui::Text(id == slot_occupant_body_id_ ? "* ID %d" : "  ID %d", id);
            if (overridden) {
                ImGui::SameLine();
                if (ImGui::SmallButton("auto")) manual_overrides_.erase(id);
            }
            ImGui::SameLine();
            // Per-body label text input. body_labels_[id] auto-creates "" if absent.
            std::string& label_ref = body_labels_[id];
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s", label_ref.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputTextWithHint("##label", "label (sender-side only)",
                                         buf, sizeof(buf))) {
                label_ref = buf;
            }
            ImGui::PopID();
        }
    }

    drawRecognitionPanel();

    ImGui::Separator();
    ImGui::Checkbox("Clean view: camera image only (h)", &clean_view_);
    ImGui::TextDisabled("Keys: n=none  a=all  [ ]=focus  Space=toggle  h=clean view");
    ImGui::End();

    // Render labels next to each body in the 2D image overlay (ImGui foreground draw list).
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImU32 shadow = IM_COL32(0, 0, 0, 200);
    // Image NDC -> window pixels (top-left origin) inside the letterboxed image area.
    const float view_top = (float)(window_h_ - view_y_ - view_h_);
    auto to_screen = [&](float nx, float ny) {
        return ImVec2(view_x_ + (nx * 0.5f + 0.5f) * view_w_,
                      view_top + (0.5f - ny * 0.5f) * view_h_);
    };
    for (int id : current_body_ids_) {
        auto ait = body_label_anchor_ndc_.find(id);
        if (ait == body_label_anchor_ndc_.end()) continue;
        ImVec2 pos = to_screen(ait->second.first, ait->second.second);
        auto lit = body_labels_.find(id);
        if (lit != body_labels_.end() && !lit->second.empty()) {
            bool sel = selected_ids_.count(id) > 0;
            ImU32 col = sel ? IM_COL32(255, 230, 0, 255) : IM_COL32(180, 180, 180, 220);
            const char* txt = lit->second.c_str();
            fg->AddText(ImVec2(pos.x + 1, pos.y + 1), shadow, txt);
            fg->AddText(pos, col, txt);
        }
        // Slot body: how many of the 13 judged joints are detected, above its head.
        if (id == slot_occupant_body_id_ && slot_joints_detected_ >= 0) {
            char count[16];
            std::snprintf(count, sizeof(count), "%d/13", slot_joints_detected_);
            float size = ImGui::GetFontSize() * 2.f;
            ImVec2 cpos(pos.x, pos.y - size - 4.f);
            ImU32 col = slot_joints_detected_ == 13 ? IM_COL32(60, 255, 60, 255) : IM_COL32(255, 80, 80, 255);
            fg->AddText(ImGui::GetFont(), size, ImVec2(cpos.x + 1, cpos.y + 1), shadow, count);
            fg->AddText(ImGui::GetFont(), size, cpos, col, count);
        }
    }

    // Trial in progress: banner at the top of the image.
    if (trial_running_) {
        float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - trial_start_).count();
        char banner[128];
        std::snprintf(banner, sizeof(banner), "REC  #%d  %s  %.1f / %.1f s", (int)trials_.size() + 1,
                      motionName(trial_current_.motion), std::min(elapsed, trial_duration_s_), trial_duration_s_);
        ImFont* font = korean_font ? korean_font : ImGui::GetFont();
        const float size = 32.f;
        ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, 0.f, banner);
        ImVec2 bpos(view_x_ + (view_w_ - text_size.x) * 0.5f, view_top + 16.f);
        fg->AddRectFilled(ImVec2(bpos.x - 12.f, bpos.y - 6.f),
                          ImVec2(bpos.x + text_size.x + 12.f, bpos.y + text_size.y + 6.f),
                          IM_COL32(0, 0, 0, 170), 6.f);
        fg->AddText(font, size, bpos, IM_COL32(255, 70, 70, 255), banner);
    }
}

void GLViewer::selectNone() {
    // Write a "force exclude" override for every visible body so it sticks past
    // the next updateData (which otherwise re-derives selected_ids_ from region).
    for (int id : current_body_ids_) manual_overrides_[id] = false;
    selected_ids_.clear();
}

void GLViewer::selectAllCurrent() {
    for (int id : current_body_ids_) manual_overrides_[id] = true;
    selected_ids_.clear();
    for (int id : current_body_ids_) selected_ids_.insert(id);
}

int GLViewer::getCurrentBodyId() const {
    if (current_body_index_ < 0 || current_body_index_ >= (int)current_body_ids_.size()) return -1;
    return current_body_ids_[current_body_index_];
}

void GLViewer::selectNextBody() {
    if (current_body_ids_.empty()) return;
    current_body_index_ = (current_body_index_ + 1) % (int)current_body_ids_.size();
}

void GLViewer::selectPrevBody() {
    if (current_body_ids_.empty()) return;
    current_body_index_ = (current_body_index_ - 1 + (int)current_body_ids_.size()) % (int)current_body_ids_.size();
}

void GLViewer::toggleCurrentSelection() {
    int id = getCurrentBodyId();
    if (id < 0) return;
    bool current = selected_ids_.find(id) != selected_ids_.end();
    manual_overrides_[id] = !current;
    if (current) selected_ids_.erase(id);
    else         selected_ids_.insert(id);
}

void GLViewer::clearInputs() {
    mouseMotion_[0] = mouseMotion_[1] = 0;
    mouseWheelPosition_ = 0;
    for (unsigned int i = 0; i < 256; ++i)
        if (keyStates_[i] != KEY_STATE::DOWN)
            keyStates_[i] = KEY_STATE::FREE;
}

void GLViewer::drawCallback() {
    currentInstance_->render();
}

void GLViewer::mouseButtonCallback(int button, int state, int x, int y) {
    ImGui_ImplGLUT_MouseFunc(button, state, x, y);
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (button < 5) {
        if (button < 3) {
            currentInstance_->mouseButton_[button] = state == GLUT_DOWN;
        }
        else {
            currentInstance_->mouseWheelPosition_ += button == MOUSE_BUTTON::WHEEL_UP ? 1 : -1;
        }
        currentInstance_->mouseCurrentPosition_[0] = x;
        currentInstance_->mouseCurrentPosition_[1] = y;
        currentInstance_->previousMouseMotion_[0] = x;
        currentInstance_->previousMouseMotion_[1] = y;
    }
}

void GLViewer::mouseMotionCallback(int x, int y) {
    ImGui_ImplGLUT_MotionFunc(x, y);
    if (ImGui::GetIO().WantCaptureMouse) return;
    currentInstance_->mouseMotion_[0] = x - currentInstance_->previousMouseMotion_[0];
    currentInstance_->mouseMotion_[1] = y - currentInstance_->previousMouseMotion_[1];
    currentInstance_->previousMouseMotion_[0] = x;
    currentInstance_->previousMouseMotion_[1] = y;
}

void GLViewer::reshapeCallback(int width, int height) {
    ImGui_ImplGLUT_ReshapeFunc(width, height);
    glViewport(0, 0, width, height);
    currentInstance_->window_w_ = width;
    currentInstance_->window_h_ = height;
    if (currentInstance_->zed_camera_set_) {
        // FOV locked to ZED intrinsics so grid/image stay aligned across window resizes.
        currentInstance_->camera_.setProjection(
            currentInstance_->zed_hfov_, currentInstance_->zed_vfov_,
            currentInstance_->camera_.getZNear(), currentInstance_->camera_.getZFar());
    } else {
        float hfov = (180.0f / M_PI) * (2.0f * atan(width / (2.0f * 500)));
        float vfov = (180.0f / M_PI) * (2.0f * atan(height / (2.0f * 500)));
        currentInstance_->camera_.setProjection(hfov, vfov, currentInstance_->camera_.getZNear(), currentInstance_->camera_.getZFar());
    }
}

void GLViewer::keyPressedCallback(unsigned char c, int x, int y) {
    ImGui_ImplGLUT_KeyboardFunc(c, x, y);
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    currentInstance_->keyStates_[c] = KEY_STATE::DOWN;
    // Selection controls (kept as keyboard fallback alongside the ImGui panel)
    if (c == 'n' || c == 'N') currentInstance_->selectNone();
    else if (c == 'a' || c == 'A') currentInstance_->selectAllCurrent();
    else if (c == '[') currentInstance_->selectPrevBody();
    else if (c == ']') currentInstance_->selectNextBody();
    else if (c == ' ') currentInstance_->toggleCurrentSelection();
    else if (c == 'h' || c == 'H') currentInstance_->clean_view_ = !currentInstance_->clean_view_;
    //glutPostRedisplay();
}

void GLViewer::keyReleasedCallback(unsigned char c, int x, int y) {
    ImGui_ImplGLUT_KeyboardUpFunc(c, x, y);
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    currentInstance_->keyStates_[c] = KEY_STATE::UP;
}

void GLViewer::idle() {
    glutPostRedisplay();
}

Simple3DObject::Simple3DObject() {
}

Simple3DObject::~Simple3DObject() {
    if (vaoID_ != 0) {
        glDeleteBuffers(4, vboID_);
        glDeleteVertexArrays(1, &vaoID_);
    }
}

Simple3DObject::Simple3DObject(sl::Translation position, bool isStatic) : isStatic_(isStatic) {
    vaoID_ = 0;
    drawingType_ = GL_TRIANGLES;
    position_ = position;
    rotation_.setIdentity();
}

void Simple3DObject::addPt(sl::float3 pt) {
    vertices_.push_back(pt.x);
    vertices_.push_back(pt.y);
    vertices_.push_back(pt.z);
}

void Simple3DObject::addClr(sl::float4 clr) {
    colors_.push_back(clr.r);
    colors_.push_back(clr.g);
    colors_.push_back(clr.b);
    colors_.push_back(clr.a);
}

void Simple3DObject::addNormal(sl::float3 normal) {
    normals_.push_back(normal.x);
    normals_.push_back(normal.y);
    normals_.push_back(normal.z);
}

void Simple3DObject::addPoints(std::vector<sl::float3> pts, sl::float4 base_clr) {
    for (int k = 0; k < pts.size(); k++) {
        sl::float3 pt = pts.at(k);
        vertices_.push_back(pt.x);
        vertices_.push_back(pt.y);
        vertices_.push_back(pt.z);
        colors_.push_back(base_clr.r);
        colors_.push_back(base_clr.g);
        colors_.push_back(base_clr.b);
        colors_.push_back(1.f);
        int current_size_index = (vertices_.size() / 3 - 1);
        indices_.push_back(current_size_index);
        indices_.push_back(current_size_index + 1);
    }
}

void Simple3DObject::addPoint(sl::float3 pt, sl::float4 clr) {
    addPt(pt);
    addClr(clr);
    indices_.push_back((int)indices_.size());
}

const std::vector<int> boxLinks = { 0, 1, 5, 4, 1, 2, 6, 5, 2, 3, 7, 6, 3, 0, 4, 7 };

void Simple3DObject::addBoundingBox(std::vector<sl::float3> bbox, sl::float4 base_clr) {

    int start_id = vertices_.size() / 3;

    float ratio = 1.f / 5.f;

    std::vector<sl::float3> bbox_;
    // generate TOP BOX
    for (int i = 0; i < 4; i++)
        bbox_.push_back(bbox[i]);
    // generate TOP BOX FADE
    for (int i = 4; i < 8; i++) {
        auto midd = bbox[i - 4] - (bbox[i - 4] - bbox[i]) * ratio;
        bbox_.push_back(midd);
    }

    // generate BOTTOM FADE
    for (int i = 4; i < 8; i++) {
        auto midd = bbox[i] + (bbox[i - 4] - bbox[i]) * ratio;
        bbox_.push_back(midd);
    }
    // generate BOTTOM BOX
    for (int i = 4; i < 8; i++)
        bbox_.push_back(bbox[i]);

    for (int i = 0; i < bbox_.size(); i++) {
        vertices_.push_back(bbox_[i].x);
        vertices_.push_back(bbox_[i].y);
        vertices_.push_back(bbox_[i].z);

        colors_.push_back(base_clr.r);
        colors_.push_back(base_clr.g);
        colors_.push_back(base_clr.b);
        colors_.push_back(((i > 3) && (i < 12)) ? 0 : base_clr.a); //fading
    }

    for (int i = 0; i < boxLinks.size(); i += 4) {
        indices_.push_back(start_id + boxLinks[i]);
        indices_.push_back(start_id + boxLinks[i + 1]);
        indices_.push_back(start_id + boxLinks[i + 2]);
        indices_.push_back(start_id + boxLinks[i + 3]);
    }

    for (int i = 0; i < boxLinks.size(); i += 4) {
        indices_.push_back(start_id + 8 + boxLinks[i]);
        indices_.push_back(start_id + 8 + boxLinks[i + 1]);
        indices_.push_back(start_id + 8 + boxLinks[i + 2]);
        indices_.push_back(start_id + 8 + boxLinks[i + 3]);
    }
}

void Simple3DObject::addLine(sl::float3 p1, sl::float3 p2, sl::float3 clr) {
    vertices_.push_back(p1.x);
    vertices_.push_back(p1.y);
    vertices_.push_back(p1.z);

    vertices_.push_back(p2.x);
    vertices_.push_back(p2.y);
    vertices_.push_back(p2.z);

    colors_.push_back(clr.r);
    colors_.push_back(clr.g);
    colors_.push_back(clr.b);
    colors_.push_back(1.f);

    colors_.push_back(clr.r);
    colors_.push_back(clr.g);
    colors_.push_back(clr.b);
    colors_.push_back(1.f);

    indices_.push_back((int)indices_.size());
    indices_.push_back((int)indices_.size());
}

void Simple3DObject::addCylinder(sl::float3 startPosition, sl::float3 endPosition, sl::float4 clr) {
    const float m_radius = 0.01f * 1000.f; //  convert to millimeters

    sl::float3 dir = endPosition - startPosition;
    float m_height = dir.norm();
    dir = dir / m_height;

    sl::float3 yAxis(0, 1, 0);
    sl::float3 v = sl::float3::cross(dir, yAxis);
    sl::Transform rotation;

    if (v.norm() < 0.00001f)
        rotation.setIdentity();
    else {
        float cosTheta = sl::float3::dot(dir, yAxis);
        float scale = (1.f - cosTheta) / (1.f - (cosTheta * cosTheta));

        float data[] = { 0, v[2], -v[1], 0,
            -v[2], 0, v[0], 0,
            v[1], -v[0], 0, 0,
            0, 0, 0, 1.f };

        sl::Transform vx = sl::Transform(data);
        rotation.setIdentity();
        rotation = rotation + vx;
        rotation = rotation + vx * vx * scale;
    }

    /////////////////////////////

    sl::float3 v1;
    sl::float3 v2;
    sl::float3 v3;
    sl::float3 v4;
    sl::float3 normal;

    const int NB_SEG = 24;
    const float scale_seg = 1.f / NB_SEG;
    auto rot = rotation.getRotationMatrix();
    for (int j = 0; j < NB_SEG; j++) {
        float i = 2.f * M_PI * (j * scale_seg);
        float i1 = 2.f * M_PI * ((j + 1) * scale_seg);
        v1 = sl::float3(m_radius * cos(i), 0, m_radius * sin(i)) * rot + startPosition;
        v2 = sl::float3(m_radius * cos(i), m_height, m_radius * sin(i)) * rot + startPosition;
        v3 = sl::float3(m_radius * cos(i1), 0, m_radius * sin(i1)) * rot + startPosition;
        v4 = sl::float3(m_radius * cos(i1), m_height, m_radius * sin(i1)) * rot + startPosition;

        addPoint(v1, clr);
        addPoint(v2, clr);
        addPoint(v4, clr);
        addPoint(v3, clr);

        normal = sl::float3::cross((v2 - v1), (v3 - v1));
        normal = normal / normal.norm();

        addNormal(normal);
        addNormal(normal);
        addNormal(normal);
        addNormal(normal);
    }
}

void Simple3DObject::addSphere(sl::float3 position, sl::float4 clr, float m_radius) {
    // const float m_radius = 0.01f * 1000.0f * 2; // convert to millimeters
    const int m_stackCount = 12;
    const int m_sectorCount = 12;

    sl::float3 point;
    sl::float3 normal;

    int i, j;
    for (i = 0; i <= m_stackCount; i++) {
        double lat0 = M_PI * (-0.5 + (double)(i - 1) / m_stackCount);
        double z0 = sin(lat0);
        double zr0 = cos(lat0);

        double lat1 = M_PI * (-0.5 + (double)i / m_stackCount);
        double z1 = sin(lat1);
        double zr1 = cos(lat1);
        for (j = 0; j <= m_sectorCount - 1; j++) {
            double lng = 2 * M_PI * (double)(j - 1) / m_sectorCount;
            double x = cos(lng);
            double y = sin(lng);

            point = sl::float3(m_radius * x * zr0, m_radius * y * zr0, m_radius * z0) + position;
            normal = sl::float3(x * zr0, y * zr0, z0);
            normal = normal / normal.norm();
            addPoint(point, clr);
            addNormal(normal);

            point = sl::float3(m_radius * x * zr1, m_radius * y * zr1, m_radius * z1) + position;
            normal = sl::float3(x * zr1, y * zr1, z1);
            normal = normal / normal.norm();
            addPoint(point, clr);
            addNormal(normal);

            lng = 2 * M_PI * (double)(j) / m_sectorCount;
            x = cos(lng);
            y = sin(lng);

            point = sl::float3(m_radius * x * zr1, m_radius * y * zr1, m_radius * z1) + position;
            normal = sl::float3(x * zr1, y * zr1, z1);
            normal = normal / normal.norm();
            addPoint(point, clr);
            addNormal(normal);

            point = sl::float3(m_radius * x * zr0, m_radius * y * zr0, m_radius * z0) + position;
            normal = sl::float3(x * zr0, y * zr0, z0);
            normal = normal / normal.norm();
            addPoint(point, clr);
            addNormal(normal);
        }
    }
}

void Simple3DObject::pushToGPU() {
    if (!isStatic_ || vaoID_ == 0) {
        if (vaoID_ == 0) {
            glGenVertexArrays(1, &vaoID_);
            glGenBuffers(4, vboID_);
        }
        glShadeModel(GL_SMOOTH);
        if (vertices_.size() > 0) {
            glBindVertexArray(vaoID_);
            glBindBuffer(GL_ARRAY_BUFFER, vboID_[Shader::ATTRIB_VERTICES_POS]);
            glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(float), &vertices_[0], isStatic_ ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
            glVertexAttribPointer(Shader::ATTRIB_VERTICES_POS, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(Shader::ATTRIB_VERTICES_POS);
        }
        if (colors_.size() > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, vboID_[Shader::ATTRIB_COLOR_POS]);
            glBufferData(GL_ARRAY_BUFFER, colors_.size() * sizeof(float), &colors_[0], isStatic_ ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
            glVertexAttribPointer(Shader::ATTRIB_COLOR_POS, 4, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(Shader::ATTRIB_COLOR_POS);
        }

        if (normals_.size() > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, vboID_[Shader::ATTRIB_NORMAL]);
            glBufferData(GL_ARRAY_BUFFER, normals_.size() * sizeof(float), &normals_[0], isStatic_ ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
            glVertexAttribPointer(Shader::ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(Shader::ATTRIB_NORMAL);
        }

        if (indices_.size() > 0) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboID_[3]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(unsigned int), &indices_[0], isStatic_ ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void Simple3DObject::clear() {
    vertices_.clear();
    colors_.clear();
    indices_.clear();
    normals_.clear();
}

void Simple3DObject::setDrawingType(GLenum type) {
    drawingType_ = type;
}

void Simple3DObject::draw() {
    if (indices_.size() && vaoID_) {
        glBindVertexArray(vaoID_);
        glDrawElements(drawingType_, (GLsizei)indices_.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void Simple3DObject::translate(const sl::Translation& t) {
    position_ = position_ + t;
}

void Simple3DObject::setPosition(const sl::Translation& p) {
    position_ = p;
}

void Simple3DObject::setRT(const sl::Transform& mRT) {
    position_ = mRT.getTranslation();
    rotation_ = mRT.getOrientation();
}

void Simple3DObject::rotate(const sl::Orientation& rot) {
    rotation_ = rot * rotation_;
}

void Simple3DObject::rotate(const sl::Rotation& m) {
    this->rotate(sl::Orientation(m));
}

void Simple3DObject::setRotation(const sl::Orientation& rot) {
    rotation_ = rot;
}

void Simple3DObject::setRotation(const sl::Rotation& m) {
    this->setRotation(sl::Orientation(m));
}

const sl::Translation& Simple3DObject::getPosition() const {
    return position_;
}

sl::Transform Simple3DObject::getModelMatrix() const {
    sl::Transform tmp;
    tmp.setOrientation(rotation_);
    tmp.setTranslation(position_);
    return tmp;
}

Shader::Shader(GLchar* vs, GLchar* fs) {
    if (!compile(verterxId_, GL_VERTEX_SHADER, vs)) {
        std::cout << "ERROR: while compiling vertex shader" << std::endl;
    }
    if (!compile(fragmentId_, GL_FRAGMENT_SHADER, fs)) {
        std::cout << "ERROR: while compiling fragment shader" << std::endl;
    }

    programId_ = glCreateProgram();

    glAttachShader(programId_, verterxId_);
    glAttachShader(programId_, fragmentId_);

    glBindAttribLocation(programId_, ATTRIB_VERTICES_POS, "in_vertex");
    glBindAttribLocation(programId_, ATTRIB_COLOR_POS, "in_texCoord");

    glLinkProgram(programId_);

    GLint errorlk(0);
    glGetProgramiv(programId_, GL_LINK_STATUS, &errorlk);
    if (errorlk != GL_TRUE) {
        std::cout << "ERROR: while linking Shader :" << std::endl;
        GLint errorSize(0);
        glGetProgramiv(programId_, GL_INFO_LOG_LENGTH, &errorSize);

        char* error = new char[errorSize + 1];
        glGetShaderInfoLog(programId_, errorSize, &errorSize, error);
        error[errorSize] = '\0';
        std::cout << error << std::endl;

        delete[] error;
        glDeleteProgram(programId_);
    }
}

Shader::~Shader() {
    if (verterxId_ != 0)
        glDeleteShader(verterxId_);
    if (fragmentId_ != 0)
        glDeleteShader(fragmentId_);
    if (programId_ != 0)
        glDeleteShader(programId_);
}

GLuint Shader::getProgramId() {
    return programId_;
}

bool Shader::compile(GLuint& shaderId, GLenum type, GLchar* src) {
    shaderId = glCreateShader(type);
    if (shaderId == 0) {
        std::cout << "ERROR: shader type (" << type << ") does not exist" << std::endl;
        return false;
    }
    glShaderSource(shaderId, 1, (const char**)&src, 0);
    glCompileShader(shaderId);

    GLint errorCp(0);
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &errorCp);
    if (errorCp != GL_TRUE) {
        std::cout << "ERROR: while compiling Shader :" << std::endl;
        GLint errorSize(0);
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &errorSize);

        char* error = new char[errorSize + 1];
        glGetShaderInfoLog(shaderId, errorSize, &errorSize, error);
        error[errorSize] = '\0';
        std::cout << error << std::endl;

        delete[] error;
        glDeleteShader(shaderId);
        return false;
    }
    return true;
}

const sl::Translation CameraGL::ORIGINAL_FORWARD = sl::Translation(0, 0, 1);
const sl::Translation CameraGL::ORIGINAL_UP = sl::Translation(0, 1, 0);
const sl::Translation CameraGL::ORIGINAL_RIGHT = sl::Translation(1, 0, 0);

CameraGL::CameraGL(sl::Translation position, sl::Translation direction, sl::Translation vertical) {
    this->position_ = position;
    setDirection(direction, vertical);

    offset_ = sl::Translation(0, 0, 0);
    view_.setIdentity();
    updateView();
    setProjection(70, 70, 200.f, 50000.f);
    updateVPMatrix();
}

CameraGL::~CameraGL() {
}

void CameraGL::update() {
    if (sl::Translation::dot(vertical_, up_) < 0)
        vertical_ = vertical_ * -1.f;
    updateView();
    updateVPMatrix();
}

void CameraGL::setProjection(float horizontalFOV, float verticalFOV, float znear, float zfar) {
    horizontalFieldOfView_ = horizontalFOV;
    verticalFieldOfView_ = verticalFOV;
    znear_ = znear;
    zfar_ = zfar;

    float fov_y = verticalFOV * M_PI / 180.f;
    float fov_x = horizontalFOV * M_PI / 180.f;

    projection_.setIdentity();
    projection_(0, 0) = 1.0f / tanf(fov_x * 0.5f);
    projection_(1, 1) = 1.0f / tanf(fov_y * 0.5f);
    projection_(2, 2) = -(zfar + znear) / (zfar - znear);
    projection_(3, 2) = -1;
    projection_(2, 3) = -(2.f * zfar * znear) / (zfar - znear);
    projection_(3, 3) = 0;
}

const sl::Transform& CameraGL::getViewProjectionMatrix() const {
    return vpMatrix_;
}

float CameraGL::getHorizontalFOV() const {
    return horizontalFieldOfView_;
}

float CameraGL::getVerticalFOV() const {
    return verticalFieldOfView_;
}

void CameraGL::setOffsetFromPosition(const sl::Translation& o) {
    offset_ = o;
}

const sl::Translation& CameraGL::getOffsetFromPosition() const {
    return offset_;
}

void CameraGL::setDirection(const sl::Translation& direction, const sl::Translation& vertical) {
    sl::Translation dirNormalized = direction;
    dirNormalized.normalize();
    this->rotation_ = sl::Orientation(ORIGINAL_FORWARD, dirNormalized * -1.f);
    updateVectors();
    this->vertical_ = vertical;
    if (sl::Translation::dot(vertical_, up_) < 0)
        rotate(sl::Rotation(M_PI, ORIGINAL_FORWARD));
}

void CameraGL::translate(const sl::Translation& t) {
    position_ = position_ + t;
}

void CameraGL::setPosition(const sl::Translation& p) {
    position_ = p;
}

void CameraGL::rotate(const sl::Orientation& rot) {
    rotation_ = rot * rotation_;
    updateVectors();
}

void CameraGL::rotate(const sl::Rotation& m) {
    this->rotate(sl::Orientation(m));
}

void CameraGL::setRotation(const sl::Orientation& rot) {
    rotation_ = rot;
    updateVectors();
}

void CameraGL::setRotation(const sl::Rotation& m) {
    this->setRotation(sl::Orientation(m));
}

const sl::Translation& CameraGL::getPosition() const {
    return position_;
}

const sl::Translation& CameraGL::getForward() const {
    return forward_;
}

const sl::Translation& CameraGL::getRight() const {
    return right_;
}

const sl::Translation& CameraGL::getUp() const {
    return up_;
}

const sl::Translation& CameraGL::getVertical() const {
    return vertical_;
}

float CameraGL::getZNear() const {
    return znear_;
}

float CameraGL::getZFar() const {
    return zfar_;
}

void CameraGL::updateVectors() {
    forward_ = ORIGINAL_FORWARD * rotation_;
    up_ = ORIGINAL_UP * rotation_;
    right_ = sl::Translation(ORIGINAL_RIGHT * -1.f) * rotation_;
}

void CameraGL::updateView() {
    sl::Transform transformation(rotation_, (offset_ * rotation_) + position_);
    view_ = sl::Transform::inverse(transformation);
}

void CameraGL::updateVPMatrix() {
    vpMatrix_ = projection_ * view_;
}
