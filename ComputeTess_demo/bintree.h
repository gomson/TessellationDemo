#ifndef BINTREE_H
#define BINTREE_H

#include "commands.h"
#include "common.h"

class BinTree
{

public:
    struct Settings
    {
        bool uniform_on;       // Toggle uniform subdivision
        int uniform_lvl;       // Level of uniform subdivision
        float lod_factor;      // Factor scaling the adaptive subdivision
        float target_length; // Target edge length on rendered grid
        bool map_nodecount;    // Toggle the readback of the node counters
        bool rotateMesh;       // Toggle mesh rotation (for mesh)
        bool displace_on;      // Toggle displacement mapping (for terrain)
        float displace_factor; // Factor for displacement mapping (for terrain)
        int color_mode;        // Switch color mode of the render
        bool MVP_on;    // Toggle the MVP matrix

        bool flat_normal; // Toggle wireframe visualisation
        bool wireframe_on; // Toggle wireframe visualisation

        int polygon_type; // Type of polygon of the mesh
        bool freeze;      // Toggle freeze = stop updating, but keep rendering
        int cpu_lod;      // Control CPU LoD, i.e. lod of the instantiated grid
        bool cull_on;     // Toggle Cull

        int itpl_type;    // Switch interpolation type
        float itpl_alpha; // Control interpolation factor

        void Upload(uint pid)
        {
            utility::SetUniformBool(pid, "u_uniform_subdiv", uniform_on);
            utility::SetUniformInt(pid, "u_uniform_level", uniform_lvl);
            utility::SetUniformFloat(pid, "u_lod_factor", lod_factor);
            utility::SetUniformFloat(pid, "u_target_edge_length", target_length);
            utility::SetUniformFloat(pid, "u_displace_factor", displace_factor);
            utility::SetUniformInt(pid, "u_color_mode", color_mode);
            utility::SetUniformBool(pid, "u_render_MVP", MVP_on);
            utility::SetUniformInt(pid, "u_cpu_lod", cpu_lod);

            utility::SetUniformFloat(pid, "u_itpl_alpha", itpl_alpha);
        }
    } settings;

    uint full_node_count, drawn_node_count;

private:
    CommandManager* commands_;

    struct ssbo_indices {
        int read = 0;
        int write_full = 1;
        int write_culled = 2;
    } ssbo_idx_;

    // Buffers and Arrays
    GLuint nodes_bo_[3];
    GLuint transfo_bo_;

    BufferCombo leaf_;

    // Mesh data
    Mesh_Data* mesh_data_;

    //Programs
    GLuint render_program_, compute_program_, copy_program_;

    //Compute Shader parameters
    uvec3 wg_local_size_;
    uint wg_local_count_;
    uint init_node_count_, wg_init_global_count_;
    int max_node_count_;

    djg_clock* compute_clock_;
    djg_clock* render_clock_;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Shader functions
    ///

    void configureComputeProgram()
    {
        utility::SetUniformInt(compute_program_, "u_num_mesh_tri",
                               mesh_data_->triangle_count);
        utility::SetUniformInt(compute_program_, "u_num_mesh_quad",
                               mesh_data_->quad_count);
        utility::SetUniformInt(compute_program_, "u_max_node_count",
                               max_node_count_);
        settings.Upload(compute_program_);
    }

    void configureCopyProgram()
    {
        utility::SetUniformInt(copy_program_, "u_num_vertices", leaf_.v.count);
        utility::SetUniformInt(copy_program_, "u_num_indices", leaf_.idx.count);

    }

    void configureRenderProgram()
    {
        settings.Upload(render_program_);
    }

    void pushMacrosToProgram(djg_program* djp)
    {
        if(settings.polygon_type == TRIANGLES)
            djgp_push_string(djp, "#define FLAG_TRIANGLES 1\n");
        else if (settings.polygon_type == QUADS)
            djgp_push_string(djp, "#define FLAG_QUADS 1\n");

        if(settings.displace_on)
            djgp_push_string(djp, "#define FLAG_DISPLACE 1\n");

        if(settings.flat_normal)
            djgp_push_string(djp, "#define FLAG_FLAT_N 1\n");

        djgp_push_string(djp, "#define TERRAIN %i\n", TERRAIN);
        djgp_push_string(djp, "#define MESH %i\n", MESH);

        djgp_push_string(djp, "#define NODES_IN_B %i\n", NODES_IN_B);
        djgp_push_string(djp, "#define NODES_OUT_FULL_B %i\n", NODES_OUT_FULL_B);
        djgp_push_string(djp, "#define NODES_OUT_CULLED_B %i\n", NODES_OUT_CULLED_B);
        djgp_push_string(djp, "#define DISPATCH_COUNTER_B %i\n", DISPATCH_INDIRECT_B);
        djgp_push_string(djp, "#define DRAW_INDIRECT_B %i\n", DRAW_INDIRECT_B);
        djgp_push_string(djp, "#define NODECOUNTER_FULL_B %i\n", NODECOUNTER_FULL_B);
        djgp_push_string(djp, "#define NODECOUNTER_CULLED_B %i\n", NODECOUNTER_CULLED_B);
        djgp_push_string(djp, "#define LEAF_VERT_B %i\n", LEAF_VERT_B);
        djgp_push_string(djp, "#define LEAF_IDX_B %i\n", LEAF_IDX_B);

        djgp_push_string(djp, "#define MESH_V_B %i\n", MESH_V_B);
        djgp_push_string(djp, "#define MESH_Q_IDX_B %i\n", MESH_Q_IDX_B);
        djgp_push_string(djp, "#define MESH_T_IDX_B %i\n", MESH_T_IDX_B);
        djgp_push_string(djp, "#define LOCAL_WG_SIZE_X %u\n", wg_local_size_.x);
        djgp_push_string(djp, "#define LOCAL_WG_SIZE_Y %u\n", wg_local_size_.y);
        djgp_push_string(djp, "#define LOCAL_WG_SIZE_Z %u\n", wg_local_size_.z);
        djgp_push_string(djp, "#define LOCAL_WG_COUNT %u\n", wg_local_count_);

    }

    bool loadComputeProgram()
    {
        cout << "Bintree - Loading Compute Program... ";
        if (!glIsProgram(compute_program_))
            compute_program_ = 0;
        djg_program* djp = djgp_create();
        pushMacrosToProgram(djp);
        if(settings.cull_on)
            djgp_push_string(djp, "#define FLAG_CULL 1\n");
        char buf[1024];
        if (settings.displace_on) {
            djgp_push_file(djp, strcat2(buf, shader_dir, "gpu_noise_lib.glsl"));
            djgp_push_file(djp, strcat2(buf, shader_dir, "noise.glsl"));
        }
        djgp_push_file(djp, strcat2(buf, shader_dir, "ltree_jk.glsl"));
        djgp_push_file(djp, strcat2(buf, shader_dir, "LoD.glsl"));
        djgp_push_file(djp, strcat2(buf, shader_dir, "bintree_compute.glsl"));
        if (!djgp_to_gl(djp, 450, false, true, &compute_program_))
        {
            cout << "X" << endl;
            djgp_release(djp);

            return false;
        }
        djgp_release(djp);
        cout << "OK" << endl;
        configureComputeProgram();
        return (glGetError() == GL_NO_ERROR);
    }

    bool loadCopyProgram()
    {
        cout << "Bintree - Loading Copy Program... ";
        if (!glIsProgram(copy_program_))
            copy_program_ = 0;
        djg_program* djp = djgp_create();
        pushMacrosToProgram(djp);

        char buf[1024];

        djgp_push_file(djp, strcat2(buf, shader_dir, "bintree_copy.glsl"));
        if (!djgp_to_gl(djp, 450, false, true, &copy_program_))
        {
            cout << "X" << endl;
            djgp_release(djp);

            return false;
        }
        djgp_release(djp);
        cout << "OK" << endl;
        configureCopyProgram();
        return (glGetError() == GL_NO_ERROR);
    }


    bool loadRenderProgram()
    {
        cout << "Bintree - Loading Render Program... ";

        if (!glIsProgram(render_program_))
            render_program_ = 0;
        djg_program *djp = djgp_create();
        pushMacrosToProgram(djp);

        djgp_push_string(djp, "#define WHITE_WIREFRAME %i\n", WHITE_WIREFRAME);
        djgp_push_string(djp, "#define PRIMITIVES %i\n", PRIMITIVES);
        djgp_push_string(djp, "#define LOD %i\n", LOD);
        djgp_push_string(djp, "#define FRUSTUM %i\n", FRUSTUM);
        djgp_push_string(djp, "#define CULL %i\n", CULL);
        djgp_push_string(djp, "#define DEBUG %i\n", DEBUG);

        if (settings.itpl_type == PN)
            djgp_push_string(djp, "#define FLAG_ITPL_PN 1\n");
        else if (settings.itpl_type == PHONG)
            djgp_push_string(djp, "#define FLAG_ITPL_PHONG 1\n");
        else
            djgp_push_string(djp, "#define FLAG_ITPL_LINEAR 1\n");

        char buf[1024];
        if (settings.displace_on) {
            djgp_push_file(djp, strcat2(buf, shader_dir, "gpu_noise_lib.glsl"));
            djgp_push_file(djp, strcat2(buf, shader_dir, "noise.glsl"));
        }
        djgp_push_file(djp, strcat2(buf, shader_dir, "ltree_jk.glsl"));
        djgp_push_file(djp, strcat2(buf, shader_dir, "LoD.glsl"));
        if (settings.itpl_type == PN)
            djgp_push_file(djp, strcat2(buf, shader_dir, "PN_interpolation.glsl"));
        else if (settings.itpl_type == PHONG)
            djgp_push_file(djp, strcat2(buf, shader_dir, "phong_interpolation.glsl"));
        djgp_push_file(djp, strcat2(buf, shader_dir, "bintree_render_common.glsl"));
        if (settings.wireframe_on)
            djgp_push_file(djp, strcat2(buf, shader_dir, "bintree_render_wireframe.glsl"));
        else
            djgp_push_file(djp, strcat2(buf, shader_dir, "bintree_render_flat.glsl"));

        if (!djgp_to_gl(djp, 450, false, true, &render_program_))
        {
            cout << "X" << endl;
            djgp_release(djp);
            return false;
        }
        djgp_release(djp);
        cout << "OK" << endl;
        configureRenderProgram();
        return (glGetError() == GL_NO_ERROR);
    }

    bool loadPrograms()
    {
        bool v = true;
        v &= loadComputeProgram();
        v &= loadCopyProgram();
        v &= loadRenderProgram();
        return v;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Buffer Function
    ///

    bool loadNodesBuffers()
    {
        int max_ssbo_size;
        glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_ssbo_size);
        max_ssbo_size /= 8;
        max_node_count_ = max_ssbo_size / sizeof(uvec4);
         cout << "max_num_nodes  " << max_node_count_ << endl;
         cout << "max_ssbo_size " << max_ssbo_size << "B" << endl;
        uvec4* nodes_array =  new uvec4[max_node_count_];
        if (settings.polygon_type == TRIANGLES) {
            init_node_count_ = mesh_data_->triangle_count;
            for (int ctr = 0; ctr < (int)init_node_count_; ++ctr) {
                nodes_array[ctr] = uvec4(0, 0x1, uint(ctr*3), 0);
            }
        } else if (settings.polygon_type == QUADS) {
            init_node_count_ = 2 * mesh_data_->quad_count;
            for (int ctr = 0; ctr < (int)init_node_count_; ++ctr) {
                nodes_array[2*ctr+0] = uvec4(0, 0x1, uint(ctr*4), 0);
                nodes_array[2*ctr+1] = uvec4(0, 0x1, uint(ctr*4), 1);
            }
        }

        utility::EmptyBuffer(&nodes_bo_[0]);
        utility::EmptyBuffer(&nodes_bo_[1]);
        utility::EmptyBuffer(&nodes_bo_[2]);

        glCreateBuffers(3, nodes_bo_);

        glNamedBufferStorage(nodes_bo_[0], max_ssbo_size, nodes_array, 0);
        glNamedBufferStorage(nodes_bo_[1], max_ssbo_size, nodes_array, 0);
        glNamedBufferStorage(nodes_bo_[2], max_ssbo_size, nodes_array, 0);

        return (glGetError() == GL_NO_ERROR);
    }

    vector<vec2> getLeafVertices(uint level)
    {
        vector<vec2> vertices;
        float num_row = 1 << level;
        float col = 0.0, row = 0.0;
        float d = 1.0 / float (num_row);
        while (row <= num_row)
        {
            while (col <= row)
            {
                vertices.push_back(vec2(col * d, 1.0 - row * d));
                col ++;
            }
            row ++;
            col = 0;
        }
        return vertices;
    }

    vector<uvec3> getLeafIndices(uint level)
    {
        vector<uvec3> indices;
        uint col = 0, row = 0;
        uint elem = 0, num_col = 1;
        uint orientation;
        uint num_row = 1 << level;
        auto new_triangle = [&]() {
            if (orientation == 0)
                return uvec3(elem, elem + num_col, elem + num_col + 1);
            else if (orientation == 1)
                return uvec3(elem, elem - 1, elem + num_col);
            else if (orientation == 2)
                return uvec3(elem, elem + num_col, elem + 1);
            else if (orientation == 3)
                return uvec3(elem, elem + num_col - 1, elem + num_col);
            else
                throw std::runtime_error("Bad orientation error");
        };
        while (row < num_row)
        {
            orientation = (row % 2 == 0) ? 0 : 2;
            while (col < num_col)
            {
                indices.push_back(new_triangle());
                orientation = (orientation + 1) % 4;
                if (col > 0) {
                    indices.push_back(new_triangle());
                    orientation = (orientation + 1) % 4;
                }
                col++;
                elem++;
            }
            col = 0;
            num_col++;
            row++;
        }
        return indices;
    }

    bool loadLeafBuffers(uint level)
    {
        vector<vec2> vertices = getLeafVertices(level);
        vector<uvec3> indices = getLeafIndices(level);

        leaf_.v.count = vertices.size();
        leaf_.v.size = leaf_.v.count * sizeof(vec2);
        utility::EmptyBuffer(&leaf_.v.bo);
        glCreateBuffers(1, &leaf_.v.bo);
        glNamedBufferStorage(leaf_.v.bo, leaf_.v.size,
                             (const void*)vertices.data(), 0);

        leaf_.idx.count = indices.size() * 3;
        leaf_.idx.size =leaf_.idx.count * sizeof(uint);
        utility::EmptyBuffer(&leaf_.idx.bo);
        glCreateBuffers(1, &leaf_.idx.bo);
        glNamedBufferStorage(leaf_.idx.bo, leaf_.idx.size,
                             (const void*)indices.data(),  0);

        return (glGetError() == GL_NO_ERROR);
    }


    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// VAO functions
    ///

    bool loadLeafVao()
    {
        if (glIsVertexArray(leaf_.vao)) {
            glDeleteVertexArrays(1, &leaf_.vao);
            leaf_.vao = 0;
        }
        glCreateVertexArrays(1, &leaf_.vao);
        glVertexArrayAttribBinding(leaf_.vao, 1, 0);
        glVertexArrayAttribFormat(leaf_.vao, 1, 2, GL_FLOAT, GL_FALSE, 0);
        glEnableVertexArrayAttrib(leaf_.vao, 1);
        glVertexArrayVertexBuffer(leaf_.vao, 0, leaf_.v.bo, 0, sizeof(vec2));
        glVertexArrayElementBuffer(leaf_.vao, leaf_.idx.bo);

        return (glGetError() == GL_NO_ERROR);
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Pingpong functions
    ///
    void pingpong()
    {
        ssbo_idx_.read = ssbo_idx_.write_full;
        ssbo_idx_.write_full = (ssbo_idx_.read + 1) % 3;
        ssbo_idx_.write_culled = (ssbo_idx_.read + 2) % 3;
    }

public:
    struct Ticks {
        double cpu;
        double gpu_compute, gpu_render;
    } ticks;
    bool capped;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Update function
    ///

    void ReloadShaders()
    {
        bool v = loadPrograms();
    }

    void ReloadRenderProgram()
    {
        loadRenderProgram();
        configureRenderProgram();
        UploadSettings();
    }

    void ReloadComputeProgram()
    {
        loadComputeProgram();
        configureComputeProgram();
        UploadSettings();
    }


    void ReconfigureShaders()
    {
        configureComputeProgram();
        configureRenderProgram();
    }

    void Reinitialize()
    {
        loadLeafBuffers(settings.cpu_lod);
        loadLeafVao();
        loadNodesBuffers();
        loadPrograms();
        commands_->Init(leaf_.idx.count, wg_init_global_count_);
    }

    void UploadSettings()
    {
        settings.Upload(compute_program_);
        settings.Upload(render_program_);
    }

    void UpdateLightPos(vec3 lp)
    {
        utility::SetUniformVec3(render_program_, "u_light_pos", lp);
    }

    void UpdateMode(uint mode)
    {
        utility::SetUniformInt(compute_program_, "u_mode", mode);
        utility::SetUniformInt(render_program_, "u_mode", mode);
    }

    void UpdateScreenRes(int s)
    {
        utility::SetUniformInt(compute_program_, "u_screen_res", s);
        utility::SetUniformInt(render_program_, "u_screen_res", s);
    }

    void UpdateLodFactor(int res, float fov) {
        float l = 2.0f * tan(glm::radians(fov) / 2.0f)
                * settings.target_length
                * (1 << settings.cpu_lod)
                / float(res);
        capped = false;
        const float cap = 0.43f;
        if (l > cap) {
            capped = true;
            l = cap;
        }
        settings.lod_factor = l / float(mesh_data_->avg_e_length);;
    }


    ////////////////////////////////////////////////////////////////////////////
    ///
    /// The Program
    ///
    ///
    /*
     * Initialize the Binary Tree:
     * - Recieve the mesh data and transform poniters
     * - Sets the settings to their initial values
     * - Generate the leaf geometry
     * - Load the buffers for the nodes and the leaf geometry
     * - Load the glsl programs
     * - Initialize the command class instance
     * - Update the uniform values once again, after all these loadings
     */
    void Init(Mesh_Data* m_data, GLuint transfo_bo, const Settings& init_settings)
    {
        cout << "******************************************************" << endl;
        cout << "BINTREE" << endl;
        mesh_data_ = m_data;
        settings = init_settings;

        commands_ = new CommandManager();
        compute_clock_ = djgc_create();
        render_clock_ = djgc_create();

        wg_local_size_ = vec3(512,1,1);
        wg_local_count_ = wg_local_size_.x * wg_local_size_.y * wg_local_size_.z;

        loadLeafBuffers(settings.cpu_lod);
        loadLeafVao();
        loadNodesBuffers();

        wg_init_global_count_ = ceil(init_node_count_ / float(wg_local_count_));

        if (!loadPrograms())
            throw std::runtime_error("shader creation error");

        transfo_bo_ = transfo_bo;
        commands_->Init(leaf_.idx.count, wg_init_global_count_);

        ReconfigureShaders();

        glUseProgram(0);
    }

    /*
     * Render function
     */
    void Draw(float deltaT)
    {
        if (settings.freeze)
            goto RENDER_PASS;

        pingpong();

        /*
         * COMPUTE PASS
         * - Reads the keys in the SSBO
         * - Evaluates the LoD
         * - Writes the new keys in opposite SSBO
         * - Performs culling
         */
        glEnable(GL_RASTERIZER_DISCARD);
        djgc_start(compute_clock_);
        glUseProgram(compute_program_);
        {
            utility::SetUniformFloat(compute_program_, "deltaT", deltaT);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, NODES_IN_B,
                             nodes_bo_[ssbo_idx_.read]);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, NODES_OUT_FULL_B,
                             nodes_bo_[ssbo_idx_.write_full]);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, NODES_OUT_CULLED_B,
                             nodes_bo_[ssbo_idx_.write_culled]);

            glBindBufferBase(GL_UNIFORM_BUFFER, 0, transfo_bo_);
            commands_->BindForCompute(compute_program_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MESH_V_B,
                             mesh_data_->v.bo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MESH_Q_IDX_B,
                             mesh_data_->q_idx.bo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MESH_T_IDX_B,
                             mesh_data_->t_idx.bo);

            glDispatchComputeIndirect((long)NULL);

            glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);
        }
        glUseProgram(0);

        /*
         * COPY PASS
         * - Reads the number of primitive written in previous Compute Pass
         * - Write the number of instances in the Draw Command Buffer
         * - Write the number of workgroups in the Dispatch Command Buffer
         */
        glUseProgram(copy_program_);
        {
            commands_->BindForCopy(copy_program_);
            glBindBufferBase(GL_UNIFORM_BUFFER, LEAF_VERT_B, leaf_.v.bo);
            glBindBufferBase(GL_UNIFORM_BUFFER, LEAF_IDX_B, leaf_.idx.bo);

            glDispatchCompute(1,1,1);
            glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
        }
        glUseProgram(0);

        djgc_stop(compute_clock_);
        djgc_ticks(compute_clock_, &ticks.cpu, &ticks.gpu_compute);

        glDisable(GL_RASTERIZER_DISCARD);
RENDER_PASS:
        if (settings.map_nodecount) {
            drawn_node_count = commands_->GetDrawnNodeCount();
            full_node_count = commands_->GetFullNodeCount();
        }
        /*
         * RENDER PASS
         * - Reads the updated keys that did not get culled
         * - Performs the morphing
         * - Render the triangles
         */
        glEnable(GL_DEPTH_TEST);
        glFrontFace(GL_CCW);

        glClearDepth(1.0);
        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(render_program_);
        {
            djgc_start(render_clock_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, NODES_IN_B,
                             nodes_bo_[ssbo_idx_.write_culled]);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MESH_V_B,
                             mesh_data_->v.bo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MESH_Q_IDX_B,
                             mesh_data_->q_idx.bo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MESH_T_IDX_B,
                             mesh_data_->t_idx.bo);
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, transfo_bo_);

            commands_->BindForRender();
            glBindVertexArray(leaf_.vao);
            glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, BUFFER_OFFSET(0));
            glBindVertexArray(0);
            djgc_stop(render_clock_);
        }
        glUseProgram(0);
        djgc_ticks(render_clock_, &ticks.cpu, &ticks.gpu_render);
    }

    void CleanUp()
    {
        glUseProgram(0);
        glDeleteBuffers(3, nodes_bo_);
        utility::EmptyBuffer(&transfo_bo_);
        glDeleteProgram(compute_program_);
        glDeleteProgram(copy_program_);
        glDeleteProgram(render_program_);
        glDeleteBuffers(1, &leaf_.v.bo);
        glDeleteBuffers(1, &leaf_.idx.bo);
        glDeleteVertexArrays(1, &leaf_.vao);
        commands_->Cleanup();
    }
};

#endif
