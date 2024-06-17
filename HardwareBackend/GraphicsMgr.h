#pragma once

#include <string>
#include <atomic>

#include "HardwareTypes.h"

#ifndef HWMGR_INCLUDE
#define HWMGR_INCLUDE
#include "../submodules/SDL/include/SDL.h"
#include "../submodules/imgui/backends/imgui_impl_sdl2.h"
#endif

namespace Backend {
	namespace Graphics {
#ifndef GRAPHICS_DEBUG
		//#define GRAPHICS_DEBUG
#endif

		const std::string SHADER_CACHE = "/cache/";

		inline const u16 ID_NVIDIA = 0x10DE;
		inline const u16 ID_AMD = 0x1002;

		inline const std::unordered_map<u16, std::string> VENDOR_IDS = {
			{0x1002, "AMD"},
			{0x10DE, "NVIDIA"},
			{0x1043, "ASUS"},
			{0x196D, "Club 3D"},
			{0x1092, "Diamond Multimedia"},
			{0x18BC, "GeCube"},
			{0x1458, "Gigabyte"},
			{0x17AF, "HIS"},
			{0x16F3, "Jetway"},
			{0x1462, "MSI"},
			{0x1DA2, "Sapphire"},
			{0x148C, "PowerColor"},
			{0x1545, "VisionTek"},
			{0x1682, "XFX"},
			{0x1025, "Acer"},
			{0x106B, "Apple"},
			{0x1028, "Dell"},
			{0x107B, "Gateway"},
			{0x103C, "HP"},
			{0x17AA, "Lenovo"},
			{0x104D, "Sony"},
			{0x1179, "Toshiba"}
		};

#define TEX2D_CHANNELS			    4

		class GraphicsMgr {

		public:
			// get/reset vkInstance
			static GraphicsMgr* getInstance(SDL_Window** _window, const graphics_settings& _settings);
			static void resetInstance();

			// render
			virtual void RenderFrame() = 0;

			// initialize
			virtual bool InitGraphics() = 0;
			virtual bool StartGraphics(bool& _present_mode_fifo, bool& _triple_buffering) = 0;

			// deinit
			virtual bool ExitGraphics() = 0;
			virtual void StopGraphics() = 0;

			// shader compilation
			virtual void EnumerateShaders() = 0;
			virtual void CompileNextShader() = 0;

			// imgui
			virtual bool InitImgui() = 0;
			virtual void DestroyImgui() = 0;
			virtual void NextFrameImGui() const = 0;

			virtual void UpdateGpuData() = 0;

			bool InitGraphicsBackend(virtual_graphics_information& _graphics_info);
			void DestroyGraphicsBackend();

			virtual void SetSwapchainSettings(bool& _present_mode_fifo, bool& _triple_buffering) = 0;

			ImFont* GetFont(const int& _index);

		protected:

			explicit GraphicsMgr(const graphics_settings& _settings) {
				title = _settings.app_title;
				vMajor = _settings.v_major;
				vMinor = _settings.v_minor;
				vPatch = _settings.v_patch;
				fontMain = _settings.font;
				shaderFolder = _settings.shader_folder;
			}
			~GraphicsMgr() = default;

			std::string title = "";
			u32 vMajor = 0;
			u32 vMinor = 0;
			u32 vPatch = 0;

			// sdl
			SDL_Window* window = nullptr;
			virtual void RecalcTex2dScaleMatrix() = 0;
			float aspectRatio = 1.f;

			virtual void UpdateTex2d() = 0;

			bool resizableBar = false;

			// gpu info
			std::string vendor = "";
			std::string driverVersion = "";

			virtual void DetectResizableBar() = 0;

			virtual bool Init2dGraphicsBackend() = 0;
			virtual void Destroy2dGraphicsBackend() = 0;

			u32 win_width = 0;
			u32 win_height = 0;

			std::string fontMain = "";

			virtual_graphics_information virtGraphicsInfo = {};

			int shadersCompiled = 0;
			int shadersTotal = 0;
			bool shaderCompilationFinished = false;

			std::string shaderFolder = "";

			std::vector<ImFont*> fonts;

		private:
			static GraphicsMgr* instance;
		};
	}
}