#ifndef __VIEWER_INCLUDE__
#define __VIEWER_INCLUDE__

#include <vector>
#include <mutex>
#include <chrono>
#include <map>
#include <deque>
#include <vector>
#include <set>
#include <string>
#include <unordered_map>
#include <sl/Camera.hpp>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <cuda.h>
#include <cuda_gl_interop.h>

#ifndef M_PI
#define M_PI 3.141592653f
#endif

#define MOUSE_R_SENSITIVITY 0.03f
#define MOUSE_UZ_SENSITIVITY 0.75f
#define MOUSE_DZ_SENSITIVITY 1.25f
#define MOUSE_T_SENSITIVITY 50.f
#define KEY_T_SENSITIVITY 0.1f

using namespace sl;

const std::vector<std::pair<BODY_38_PARTS, BODY_38_PARTS>> BODY_BONES_FAST_RENDER
{
    {
        BODY_38_PARTS::PELVIS, BODY_38_PARTS::SPINE_1
    },
    {
        BODY_38_PARTS::SPINE_1, BODY_38_PARTS::SPINE_2
    },
    {
        BODY_38_PARTS::SPINE_2, BODY_38_PARTS::SPINE_3
    },
    {
        BODY_38_PARTS::SPINE_3, BODY_38_PARTS::NECK
    },
    // Face
    {
        BODY_38_PARTS::NECK, BODY_38_PARTS::NOSE
    },
    {
        BODY_38_PARTS::NOSE, BODY_38_PARTS::LEFT_EYE
    },
    {
        BODY_38_PARTS::LEFT_EYE, BODY_38_PARTS::LEFT_EAR
    },
    {
        BODY_38_PARTS::NOSE, BODY_38_PARTS::RIGHT_EYE
    },
    {
        BODY_38_PARTS::RIGHT_EYE, BODY_38_PARTS::RIGHT_EAR
    },
    // Left arm
    {
        BODY_38_PARTS::SPINE_3, BODY_38_PARTS::LEFT_CLAVICLE
    },
    {
        BODY_38_PARTS::LEFT_CLAVICLE, BODY_38_PARTS::LEFT_SHOULDER
    },
    {
        BODY_38_PARTS::LEFT_SHOULDER, BODY_38_PARTS::LEFT_ELBOW
    },
    {
        BODY_38_PARTS::LEFT_ELBOW, BODY_38_PARTS::LEFT_WRIST
    },
    // Right arm
    {
        BODY_38_PARTS::SPINE_3, BODY_38_PARTS::RIGHT_CLAVICLE
    },
    {
        BODY_38_PARTS::RIGHT_CLAVICLE, BODY_38_PARTS::RIGHT_SHOULDER
    },
    {
        BODY_38_PARTS::RIGHT_SHOULDER, BODY_38_PARTS::RIGHT_ELBOW
    },
    {
        BODY_38_PARTS::RIGHT_ELBOW, BODY_38_PARTS::RIGHT_WRIST
    },
    // Left leg
    {
        BODY_38_PARTS::PELVIS, BODY_38_PARTS::LEFT_HIP
    },
    {
        BODY_38_PARTS::LEFT_HIP, BODY_38_PARTS::LEFT_KNEE
    },
    {
        BODY_38_PARTS::LEFT_KNEE, BODY_38_PARTS::LEFT_ANKLE
    },
    {
        BODY_38_PARTS::LEFT_ANKLE, BODY_38_PARTS::LEFT_HEEL
    },
    {
        BODY_38_PARTS::LEFT_ANKLE, BODY_38_PARTS::LEFT_BIG_TOE
    },
    {
        BODY_38_PARTS::LEFT_ANKLE, BODY_38_PARTS::LEFT_SMALL_TOE
    },
    // Right leg
    {
        BODY_38_PARTS::PELVIS, BODY_38_PARTS::RIGHT_HIP
    },
    {
        BODY_38_PARTS::RIGHT_HIP, BODY_38_PARTS::RIGHT_KNEE
    },
    {
        BODY_38_PARTS::RIGHT_KNEE, BODY_38_PARTS::RIGHT_ANKLE
    },
    {
        BODY_38_PARTS::RIGHT_ANKLE, BODY_38_PARTS::RIGHT_HEEL
    },
    {
        BODY_38_PARTS::RIGHT_ANKLE, BODY_38_PARTS::RIGHT_BIG_TOE
    },
    {
        BODY_38_PARTS::RIGHT_ANKLE, BODY_38_PARTS::RIGHT_SMALL_TOE
    },
};

///////////////////////////////////////////////////////////////////////////////////////////////

class Shader {
public:

    Shader() {
    }
    Shader(GLchar* vs, GLchar* fs);
    ~Shader();
    GLuint getProgramId();

    static const GLint ATTRIB_VERTICES_POS = 0;
    static const GLint ATTRIB_COLOR_POS = 1;
    static const GLint ATTRIB_NORMAL = 2;
private:
    bool compile(GLuint& shaderId, GLenum type, GLchar* src);
    GLuint verterxId_;
    GLuint fragmentId_;
    GLuint programId_;
};

struct ShaderData {
    Shader it;
    GLuint MVP_Mat;
};

class Simple3DObject {
public:

    Simple3DObject();
    Simple3DObject(sl::Translation position, bool isStatic);
    ~Simple3DObject();

    void addPt(sl::float3 pt);
    void addClr(sl::float4 clr);
    void addNormal(sl::float3 normal);
    void addPoints(std::vector<sl::float3> pts, sl::float4 base_clr);
    void addBoundingBox(std::vector<sl::float3> bbox, sl::float4 base_clr);
    void addPoint(sl::float3 pt, sl::float4 clr);
    void addLine(sl::float3 p1, sl::float3 p2, sl::float3 clr);
    void addCylinder(sl::float3 startPosition, sl::float3 endPosition, sl::float4 clr);
    void addSphere(sl::float3 position, sl::float4 clr, float radius = 0.01f * 1000.0f * 2);
    void pushToGPU();
    void clear();

    void setDrawingType(GLenum type);

    void draw();

    void translate(const sl::Translation& t);
    void setPosition(const sl::Translation& p);

    void setRT(const sl::Transform& mRT);

    void rotate(const sl::Orientation& rot);
    void rotate(const sl::Rotation& m);
    void setRotation(const sl::Orientation& rot);
    void setRotation(const sl::Rotation& m);

    const sl::Translation& getPosition() const;

    sl::Transform getModelMatrix() const;

private:
    std::vector<float> vertices_;
    std::vector<float> colors_;
    std::vector<unsigned int> indices_;
    std::vector<float> normals_;

    bool isStatic_;

    GLenum drawingType_;

    GLuint vaoID_;
    /*
    Vertex buffer IDs:
    - [0]: Vertices coordinates;
    - [1]: Colors;
    - [2]: Indices;
    - [3]: Normals
     */
    GLuint vboID_[4];

    sl::Translation position_;
    sl::Orientation rotation_;
};

class CameraGL {
public:

    CameraGL() {
    }

    enum DIRECTION {
        UP, DOWN, LEFT, RIGHT, FORWARD, BACK
    };
    CameraGL(sl::Translation position, sl::Translation direction, sl::Translation vertical = sl::Translation(0, 1, 0)); // vertical = Eigen::Vector3f(0, 1, 0)
    ~CameraGL();

    void update();
    void setProjection(float horizontalFOV, float verticalFOV, float znear, float zfar);
    const sl::Transform& getViewProjectionMatrix() const;

    float getHorizontalFOV() const;
    float getVerticalFOV() const;

    // Set an offset between the eye of the camera and its position
    // Note: Useful to use the camera as a trackball camera with z>0 and x = 0, y = 0
    // Note: coordinates are in local space
    void setOffsetFromPosition(const sl::Translation& offset);
    const sl::Translation& getOffsetFromPosition() const;

    void setDirection(const sl::Translation& direction, const sl::Translation& vertical);
    void translate(const sl::Translation& t);
    void setPosition(const sl::Translation& p);
    void rotate(const sl::Orientation& rot);
    void rotate(const sl::Rotation& m);
    void setRotation(const sl::Orientation& rot);
    void setRotation(const sl::Rotation& m);

    const sl::Translation& getPosition() const;
    const sl::Translation& getForward() const;
    const sl::Translation& getRight() const;
    const sl::Translation& getUp() const;
    const sl::Translation& getVertical() const;
    float getZNear() const;
    float getZFar() const;

    static const sl::Translation ORIGINAL_FORWARD;
    static const sl::Translation ORIGINAL_UP;
    static const sl::Translation ORIGINAL_RIGHT;

    sl::Transform projection_;
    bool usePerspective_;
private:
    void updateVectors();
    void updateView();
    void updateVPMatrix();

    sl::Translation offset_;
    sl::Translation position_;
    sl::Translation forward_;
    sl::Translation up_;
    sl::Translation right_;
    sl::Translation vertical_;

    sl::Orientation rotation_;

    sl::Transform view_;
    sl::Transform vpMatrix_;
    float horizontalFieldOfView_;
    float verticalFieldOfView_;
    float znear_;
    float zfar_;
};

// This class manages input events, window and Opengl rendering pipeline

class GLViewer {
public:
    GLViewer();
    ~GLViewer();
    bool isAvailable();
    void init(int argc, char** argv);
    void updateData(Bodies&, sl::Transform&);
    void updateImage(sl::Mat& image);
    void exit();
    // Selection API for main loop
    std::vector<int> getSelectedIds() const;
    void selectNone();
    void selectAllCurrent();
    void selectNextBody();
    void selectPrevBody();
    void toggleCurrentSelection();
    // Sender-side label (shown in panel + 2D overlay; not transmitted)
    std::string getLabel(int body_id) const;
    // Align the GL viewer camera to the ZED pose + intrinsics so the 3D floor grid
    // overlays exactly on the floor in the 2D image. Call once after first grab.
    void setZEDCameraPose(const sl::Transform& pose, float hfov_deg, float vfov_deg);
    // Region / subject id / slot grace period from ZEDLiveLinkConfig.json (SelectionParameters).
    // The region given here is also what "Reset region" restores.
    void setSelectionDefaults(float x_min_m, float x_max_m, float z_min_m, float z_max_m,
                              int subject_id, float slot_grace_s);
    // Output slot: a single body claims the slot and is streamed under a fixed
    // Unreal subject id. main.cpp reads these to drive the send loop.
    int  getSlotBodyId() const     { return slot_occupant_body_id_; }
    int  getFixedSubjectId() const { return fixed_subject_id_; }
    void releaseSlot()             { slot_occupant_body_id_ = -1; slot_occupant_lost_ = false; }

private:
    void render();
    void update();
    void draw();
    void clearInputs();
    int getCurrentBodyId() const;
    void initImGui();
    void drawImGuiPanel();
    void shutdownImGui();
    void applyZEDCameraPose();   // restore camera_ to the stored ZED pose + FOV
    void rebuildRegionOutline(); // refresh region_outline_ geometry when edges change
    void updateImageViewport();  // letterbox the image area to the camera aspect ratio
    // Recognition test: 13 judged joints of the slot body, live and over timed trials.
    void evaluateRecognition(Bodies& bodies);
    void drawRecognitionPanel();
    void startTrial();
    void finishTrial(std::chrono::steady_clock::time_point now);

    // Glut functions callbacks
    static void drawCallback();
    static void mouseButtonCallback(int button, int state, int x, int y);
    static void mouseMotionCallback(int x, int y);
    static void reshapeCallback(int width, int height);
    static void keyPressedCallback(unsigned char c, int x, int y);
    static void keyReleasedCallback(unsigned char c, int x, int y);
    static void idle();

    bool available;

    enum MOUSE_BUTTON {
        LEFT = 0,
        MIDDLE = 1,
        RIGHT = 2,
        WHEEL_UP = 3,
        WHEEL_DOWN = 4
    };

    enum KEY_STATE {
        UP = 'u',
        DOWN = 'd',
        FREE = 'f'
    };

    bool mouseButton_[3];
    int mouseWheelPosition_;
    int mouseCurrentPosition_[2];
    int mouseMotion_[2];
    int previousMouseMotion_[2];
    KEY_STATE keyStates_[256];

    std::mutex mtx;

    ShaderData shaderLine;
    ShaderData shaderSK;
    ShaderData shaderTex;

    sl::float3 bckgrnd_clr;

    CameraGL camera_;

    Simple3DObject skeletons;
    Simple3DObject floor_grid;
    Simple3DObject overlay2d;
    Simple3DObject region_outline_;

    sl::Transform cam_pose;

    // Image quad (full-screen textured background)
    GLuint image_vao_ = 0;
    GLuint image_vbo_[2] = {0, 0}; // 0: positions, 1: uvs
    GLuint image_tex_id_ = 0;
    int image_w_ = 0;
    int image_h_ = 0;
    bool has_image_ = false;
    bool draw_3d_skeletons_ = false;
    // Selection state
    std::set<int> selected_ids_;
    std::vector<int> current_body_ids_;
    int current_body_index_ = -1;
    // Sender-side labels for each body id (persistent across detection gaps)
    std::unordered_map<int, std::string> body_labels_;
    // Pelvis screen position (NDC) cached per visible body for label drawing
    std::unordered_map<int, std::pair<float, float>> body_label_anchor_ndc_;
    bool imgui_initialized_ = false;
    // ZED camera alignment for grid/image overlay
    sl::Transform zed_camera_pose_;
    float zed_hfov_ = 0.f;
    float zed_vfov_ = 0.f;
    bool  zed_camera_set_ = false;
    // Tracking region (world-space meters, y=0 plane). Defaults to 4m x 6m centered on origin.
    float region_x_min_m_ = -2.0f;
    float region_x_max_m_ =  2.0f;
    float region_z_min_m_ = -3.0f;
    float region_z_max_m_ =  3.0f;
    bool  region_outline_dirty_ = true;
    // Sticky per-body manual override: id -> forced selected state.
    // Absence of entry => follow auto (region) decision.
    std::unordered_map<int, bool> manual_overrides_;
    // Cached this-frame in-region status per body id, for ImGui panel badges.
    std::unordered_map<int, bool> body_in_region_;
    // Region restored by "Reset region" (set from the config file).
    float region_default_x_min_m_ = -2.0f;
    float region_default_x_max_m_ =  2.0f;
    float region_default_z_min_m_ = -3.0f;
    float region_default_z_max_m_ =  3.0f;
    // Single output slot — one body at a time is streamed to Unreal under a fixed subject id.
    int slot_occupant_body_id_ = -1;   // -1 = slot empty
    int fixed_subject_id_      = 1;    // numeric id sent to Unreal as the LiveLink subject name
    // Grace period: a slot body whose tracking is briefly lost (SEARCHING) keeps the slot
    // this long before another body may claim it.
    float slot_grace_s_ = 1.0f;
    bool  slot_occupant_lost_ = false;
    std::chrono::steady_clock::time_point slot_lost_since_;
    // Clean view: camera image only (no panel, grid or overlays), for side-by-side recording.
    bool clean_view_ = false;
    // Window size and the letterboxed image area inside it (GL coordinates, bottom-left origin).
    int window_w_ = 0, window_h_ = 0;
    int view_x_ = 0, view_y_ = 0, view_w_ = 0, view_h_ = 0;
    // Recognition test: are the 13 judged joints (head, shoulders, elbows, wrists, hips, knees,
    // ankles) of the slot body detected? Evaluated live and over timed trials, one per motion.
    struct RecognitionTrial {
        int   motion = 0;             // index into the 10 test motions
        int   frames = 0;             // new body tracking results seen during the trial
        int   frames_all_joints = 0;  // of those, results with all 13 joints on the slot body
        float longest_gap_s = 0.f;    // longest stretch without all 13 joints
        bool  success = false;
    };
    bool  slot_joint_detected_[13] = {};
    int   slot_joints_detected_ = -1;  // -1: no slot body (or not BODY_38) this frame
    bool  trial_running_ = false;
    int   trial_motion_ = 0;            // motion of the next trial
    float trial_duration_s_ = 5.0f;
    float trial_pass_ratio_ = 0.95f;    // share of results that must show all 13 joints
    std::chrono::steady_clock::time_point trial_start_;
    bool  trial_gap_open_ = false;
    std::chrono::steady_clock::time_point trial_gap_start_;
    RecognitionTrial trial_current_;
    std::vector<RecognitionTrial> trials_;
    std::string trial_csv_path_;
};

#endif /* __VIEWER_INCLUDE__ */
