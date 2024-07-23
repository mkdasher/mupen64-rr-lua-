#pragma once
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <Windows.h>
#include <assert.h>
#include <xxh64.h>

namespace LuaCore::D2D
{
	typedef struct
	{
		uint64_t text_hash;
		uint64_t font_name_hash;
		int font_weight;
		int font_style;
		float font_size;
		int horizontal_alignment;
		int vertical_alignment;
		float width;
		float height;
	} t_text_layout_params;


#define D2D_GET_RECT(L, idx) D2D1::RectF( \
	luaL_checknumber(L, idx), \
	luaL_checknumber(L, idx + 1), \
	luaL_checknumber(L, idx + 2), \
	luaL_checknumber(L, idx + 3) \
)

#define D2D_GET_COLOR(L, idx) D2D1::ColorF( \
	luaL_checknumber(L, idx), \
	luaL_checknumber(L, idx + 1), \
	luaL_checknumber(L, idx + 2), \
	luaL_checknumber(L, idx + 3) \
)

#define D2D_GET_POINT(L, idx) D2D1_POINT_2F{ \
	.x = (float)luaL_checknumber(L, idx), \
	.y = (float)luaL_checknumber(L, idx + 1) \
}

#define D2D_GET_ELLIPSE(L, idx) D2D1_ELLIPSE{ \
	.point = D2D_GET_POINT(L, idx), \
	.radiusX = (float)luaL_checknumber(L, idx + 2), \
	.radiusY = (float)luaL_checknumber(L, idx + 3) \
}

#define D2D_GET_ROUNDED_RECT(L, idx) D2D1_ROUNDED_RECT( \
	D2D_GET_RECT(L, idx), \
	luaL_checknumber(L, idx + 5), \
	luaL_checknumber(L, idx + 6) \
)

	static int create_brush(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1::ColorF color = D2D_GET_COLOR(L, 1);

		ID2D1SolidColorBrush* brush;
		lua->d2d_render_target_stack.top()->CreateSolidColorBrush(
			color,
			&brush
		);

		lua_pushinteger(L, (uint64_t)brush);
		return 1;
	}

	static int free_brush(lua_State* L)
	{
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 1);
		brush->Release();
		return 0;
	}

	static int clear(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1::ColorF color = D2D_GET_COLOR(L, 1);

		lua->d2d_render_target_stack.top()->Clear(lua->presenter->adjust_clear_color(color));

		return 0;
	}

	static int fill_rectangle(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F rectangle = D2D_GET_RECT(L, 1);
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 5);

		lua->d2d_render_target_stack.top()->FillRectangle(&rectangle, brush);

		return 0;
	}

	static int draw_rectangle(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F rectangle = D2D_GET_RECT(L, 1);
		float thickness = luaL_checknumber(L, 5);
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 6);

		lua->d2d_render_target_stack.top()->DrawRectangle(
			&rectangle, brush, thickness);

		return 0;
	}

	static int fill_ellipse(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_ELLIPSE ellipse = D2D_GET_ELLIPSE(L, 1);
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 5);

		lua->d2d_render_target_stack.top()->FillEllipse(&ellipse, brush);

		return 0;
	}

	static int draw_ellipse(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_ELLIPSE ellipse = D2D_GET_ELLIPSE(L, 1);
		float thickness = luaL_checknumber(L, 5);
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 6);

		lua->d2d_render_target_stack.top()->DrawEllipse(
			&ellipse, brush, thickness);

		return 0;
	}

	static int draw_line(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_POINT_2F point_a = D2D_GET_POINT(L, 1);
		D2D1_POINT_2F point_b = D2D_GET_POINT(L, 3);
		float thickness = luaL_checknumber(L, 5);
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 6);

		lua->d2d_render_target_stack.top()->DrawLine(
			point_a, point_b, brush, thickness);

		return 0;
	}


	static int draw_text(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F rectangle = D2D_GET_RECT(L, 1);
		auto text = std::string(luaL_checkstring(L, 5));
		auto font_name = std::string(luaL_checkstring(L, 6));
		auto font_size = static_cast<float>(luaL_checknumber(L, 7));
		auto font_weight = static_cast<int>(luaL_checknumber(L, 8));
		auto font_style = static_cast<int>(luaL_checkinteger(L, 9));
		auto horizontal_alignment = static_cast<int>(luaL_checkinteger(L, 10));
		auto vertical_alignment = static_cast<int>(luaL_checkinteger(L, 11));
		int options = luaL_checkinteger(L, 12);
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 13);

		uint64_t font_name_hash = xxh64::hash(font_name.data(), font_name.size(), 0);
		uint64_t text_hash = xxh64::hash(text.data(), text.size(), 0);

		t_text_layout_params params = {
			.text_hash = text_hash,
			.font_name_hash = font_name_hash,
			.font_weight = font_weight,
			.font_style = font_style,
			.font_size = font_size,
			.horizontal_alignment = horizontal_alignment,
			.vertical_alignment = vertical_alignment,
			.width = rectangle.right - rectangle.left,
			.height = rectangle.bottom - rectangle.top,
		};

		uint64_t params_hash = xxh64::hash((const char*)&params, sizeof(params), 0);

		if (!lua->dw_text_layouts.contains(params_hash))
		{
			// printf("[Lua] Adding layout to cache... (%d elements)\n", lua->dw_text_layouts.size());

			IDWriteTextFormat* text_format;

			lua->dw_factory->CreateTextFormat(
				string_to_wstring(font_name).c_str(),
				nullptr,
				static_cast<DWRITE_FONT_WEIGHT>(font_weight),
				static_cast<DWRITE_FONT_STYLE>(font_style),
				DWRITE_FONT_STRETCH_NORMAL,
				font_size,
				L"",
				&text_format
			);

			text_format->SetTextAlignment(
				static_cast<DWRITE_TEXT_ALIGNMENT>(horizontal_alignment));
			text_format->SetParagraphAlignment(
				static_cast<DWRITE_PARAGRAPH_ALIGNMENT>(vertical_alignment));

			IDWriteTextLayout* text_layout;

			auto wtext = string_to_wstring(text);
			lua->dw_factory->CreateTextLayout(wtext.c_str(), wtext.length(),
			                                  text_format,
			                                  rectangle.right - rectangle.left,
			                                  rectangle.bottom - rectangle.top,
			                                  &text_layout);

			lua->dw_text_layouts.add(params_hash, text_layout);
			text_format->Release();
		}

		auto layout = lua->dw_text_layouts.get(params_hash);
		lua->d2d_render_target_stack.top()->DrawTextLayout({
			                                                   .x = rectangle.left,
			                                                   .y = rectangle.top,
		                                                   }, layout.value(), brush,
		                                                   static_cast<
			                                                   D2D1_DRAW_TEXT_OPTIONS>(
			                                                   options));

		return 0;
	}

	static int set_text_antialias_mode(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		float mode = luaL_checkinteger(L, 1);
		lua->d2d_render_target_stack.top()->SetTextAntialiasMode(
			(D2D1_TEXT_ANTIALIAS_MODE)mode);
		return 0;
	}

	static int set_antialias_mode(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		float mode = luaL_checkinteger(L, 1);
		lua->d2d_render_target_stack.top()->SetAntialiasMode(
			(D2D1_ANTIALIAS_MODE)mode);
		return 0;
	}

	static int measure_text(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		std::wstring text = string_to_wstring(
			std::string(luaL_checkstring(L, 1)));
		std::string font_name = std::string(luaL_checkstring(L, 2));
		float font_size = luaL_checknumber(L, 3);
		float max_width = luaL_checknumber(L, 4);
		float max_height = luaL_checknumber(L, 5);

		IDWriteTextFormat* text_format;

		lua->dw_factory->CreateTextFormat(
			string_to_wstring(font_name).c_str(),
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			font_size,
			L"",
			&text_format
		);

		IDWriteTextLayout* text_layout;

		lua->dw_factory->CreateTextLayout(text.c_str(), text.length(),
		                                  text_format, max_width, max_height,
		                                  &text_layout);

		DWRITE_TEXT_METRICS text_metrics;
		text_layout->GetMetrics(&text_metrics);

		lua_newtable(L);
		lua_pushinteger(L, text_metrics.widthIncludingTrailingWhitespace);
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, text_metrics.height);
		lua_setfield(L, -2, "height");

		text_format->Release();
		text_layout->Release();

		return 1;
	}

	static int push_clip(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F rectangle = D2D_GET_RECT(L, 1);

		lua->d2d_render_target_stack.top()->PushAxisAlignedClip(
			rectangle,
			D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
		);

		return 0;
	}

	static int pop_clip(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		lua->d2d_render_target_stack.top()->PopAxisAlignedClip();

		return 0;
	}

	static int fill_rounded_rectangle(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_ROUNDED_RECT rounded_rectangle = D2D_GET_ROUNDED_RECT(L, 1);
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 7);

		lua->d2d_render_target_stack.top()->FillRoundedRectangle(
			&rounded_rectangle, brush);

		return 0;
	}

	static int draw_rounded_rectangle(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_ROUNDED_RECT rounded_rectangle = D2D_GET_ROUNDED_RECT(L, 1);
		float thickness = luaL_checknumber(L, 7);
		auto brush = (ID2D1SolidColorBrush*)luaL_checkinteger(L, 8);

		lua->d2d_render_target_stack.top()->DrawRoundedRectangle(
			&rounded_rectangle, brush, thickness);

		return 0;
	}

	static int load_image(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		std::string path(luaL_checkstring(L, 1));

		IWICImagingFactory* pIWICFactory = NULL;
		IWICBitmapDecoder* pDecoder = NULL;
		IWICBitmapFrameDecode* pSource = NULL;
		IWICFormatConverter* pConverter = NULL;
		ID2D1Bitmap* bmp = NULL;

		CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pIWICFactory)
		);

		HRESULT hr = pIWICFactory->CreateDecoderFromFilename(
			string_to_wstring(path).c_str(),
			NULL,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			&pDecoder
		);

		if (!SUCCEEDED(hr))
		{
			printf("D2D image fail HRESULT %d\n", hr);
			pIWICFactory->Release();
			return 0;
		}

		pIWICFactory->CreateFormatConverter(&pConverter);
		pDecoder->GetFrame(0, &pSource);
		pConverter->Initialize(
			pSource,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			NULL,
			0.0f,
			WICBitmapPaletteTypeMedianCut
		);

		lua->d2d_render_target_stack.top()->CreateBitmapFromWicBitmap(
			pConverter,
			NULL,
			&bmp
		);

		pIWICFactory->Release();
		pDecoder->Release();
		pSource->Release();
		pConverter->Release();

		lua_pushinteger(L, (uint64_t)bmp);
		return 1;
	}

	static int free_image(lua_State* L)
	{
		auto bmp = (ID2D1Bitmap*)luaL_checkinteger(L, 1);
		bmp->Release();
		return 0;
	}

	static int draw_image(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F destination_rectangle = D2D_GET_RECT(L, 1);
		D2D1_RECT_F source_rectangle = D2D_GET_RECT(L, 5);
		float opacity = luaL_checknumber(L, 9);
		int interpolation = luaL_checkinteger(L, 10);
		auto bmp = (ID2D1Bitmap*)luaL_checkinteger(L, 11);

		lua->d2d_render_target_stack.top()->DrawBitmap(
			bmp,
			destination_rectangle,
			opacity,
			(D2D1_BITMAP_INTERPOLATION_MODE)interpolation,
			source_rectangle
		);

		return 0;
	}

	static int get_image_info(lua_State* L)
	{
		auto bmp = (ID2D1Bitmap*)luaL_checkinteger(L, 1);

		D2D1_SIZE_U size = bmp->GetPixelSize();
		lua_newtable(L);
		lua_pushinteger(L, size.width);
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, size.height);
		lua_setfield(L, -2, "height");

		return 1;
	}

	static int draw_to_image(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		float width = luaL_checknumber(L, 1);
		float height = luaL_checknumber(L, 2);

		ID2D1BitmapRenderTarget* render_target;
		lua->d2d_render_target_stack.top()->CreateCompatibleRenderTarget(
			D2D1::SizeF(width, height), &render_target);

		// With render target at top of stack, we hand control back to script and let it run its callback with rt-scoped drawing
		lua->d2d_render_target_stack.push(render_target);
		render_target->BeginDraw();
		render_target->Clear(D2D1::ColorF(0, 0, 0, 0));
		lua_call(L, 0, 0);
		render_target->EndDraw();
		lua->d2d_render_target_stack.pop();

		ID2D1Bitmap* bmp;
		render_target->GetBitmap(&bmp);

		render_target->Release();
		lua_pushinteger(L, (uint64_t)bmp);
		return 1;
	}

#undef D2D_GET_RECT
#undef D2D_GET_COLOR
#undef D2D_GET_POINT
#undef D2D_GET_ELLIPS
#undef D2D_GET_ROUNDED_RECT
}
