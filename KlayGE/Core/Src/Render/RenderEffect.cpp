// RenderEffect.cpp
// KlayGE 渲染效果类 实现文件
// Ver 3.11.0
// 版权所有(C) 龚敏敏, 2003-2010
// Homepage: http://www.klayge.org
//
// 3.11.0
// Thead-safe singleton (2010.7.6)
//
// 3.9.0
// 直接从fxml文件读取特效脚本 (2009.4.21)
// 支持Buffer类型 (2009.5.14)
// 支持macro标签 (2009.5.22)
//
// 3.8.0
// 支持CBuffer (2008.10.6)
//
// 3.6.0
// 增加了Clone (2007.6.11)
//
// 3.5.0
// 改用基于xml的特效格式 (2006.10.21)
//
// 3.2.0
// 支持了bool类型 (2006.3.8)
//
// 3.0.0
// 增加了RenderTechnique和RenderPass (2005.9.4)
//
// 2.8.0
// 增加了Do*函数，使用模板方法模式 (2005.7.24)
// 使用新的自动更新参数的方法 (2005.7.25)
//
// 2.2.0
// 统一使用istream作为资源标示符 (2004.10.26)
//
// 2.1.2
// 增加了Parameter (2004.5.26)
//
// 2.0.3
// 初次建立 (2003.3.2)
// 修改了SetTexture的参数 (2004.3.6)
//
// 修改记录
//////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KFL/ErrorHandling.hpp>
#include <KFL/Util.hpp>
#include <KlayGE/ResLoader.hpp>
#include <KlayGE/Context.hpp>
#include <KFL/Math.hpp>
#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/RenderStateObject.hpp>
#include <KlayGE/ShaderObject.hpp>
#include <KFL/XMLDom.hpp>
#include <KFL/Thread.hpp>
#include <KFL/Hash.hpp>

#include <fstream>
#include <boost/assert.hpp>
#if defined(KLAYGE_COMPILER_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // Ignore auto_ptr declaration
#endif
#include <boost/algorithm/string/split.hpp>
#if defined(KLAYGE_COMPILER_GCC)
#pragma GCC diagnostic pop
#endif
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

#include <KlayGE/RenderEffect.hpp>

namespace
{
	using namespace KlayGE;

	uint32_t const KFX_VERSION = 0x0110;

	std::mutex singleton_mutex;

	class type_define
	{
	public:
		static type_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<type_define>();
				}
			}
			return *instance_;
		}

		uint32_t TypeCode(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < types_hash_.size(); ++ i)
			{
				if (types_hash_[i] == name_hash)
				{
					return i;
				}
			}

			KFL_UNREACHABLE("Invalid type name");
		}

		std::string const & TypeName(uint32_t code) const
		{
			if (code < types_.size())
			{
				return types_[code];
			}

			KFL_UNREACHABLE("Invalid type code");
		}

		type_define()
		{
			types_.push_back("bool");
			types_.push_back("string");
			types_.push_back("texture1D");
			types_.push_back("texture2D");
			types_.push_back("texture3D");
			types_.push_back("textureCUBE");
			types_.push_back("texture1DArray");
			types_.push_back("texture2DArray");
			types_.push_back("texture3DArray");
			types_.push_back("textureCUBEArray");
			types_.push_back("sampler");
			types_.push_back("shader");
			types_.push_back("uint");
			types_.push_back("uint2");
			types_.push_back("uint3");
			types_.push_back("uint4");
			types_.push_back("int");
			types_.push_back("int2");
			types_.push_back("int3");
			types_.push_back("int4");
			types_.push_back("float");
			types_.push_back("float2");
			types_.push_back("float2x2");
			types_.push_back("float2x3");
			types_.push_back("float2x4");
			types_.push_back("float3");
			types_.push_back("float3x2");
			types_.push_back("float3x3");
			types_.push_back("float3x4");
			types_.push_back("float4");
			types_.push_back("float4x2");
			types_.push_back("float4x3");
			types_.push_back("float4x4");
			types_.push_back("buffer");
			types_.push_back("structured_buffer");
			types_.push_back("byte_address_buffer");
			types_.push_back("rw_buffer");
			types_.push_back("rw_structured_buffer");
			types_.push_back("rw_texture1D");
			types_.push_back("rw_texture2D");
			types_.push_back("rw_texture3D");
			types_.push_back("rw_texture1DArray");
			types_.push_back("rw_texture2DArray");
			types_.push_back("rw_byte_address_buffer");
			types_.push_back("append_structured_buffer");
			types_.push_back("consume_structured_buffer");

			types_hash_.resize(types_.size());
			for (size_t i = 0; i < types_.size(); ++ i)
			{
				types_hash_[i] = RT_HASH(types_[i].c_str());
			}
		}

	private:
		std::vector<std::string> types_;
		std::vector<size_t> types_hash_;

		static std::unique_ptr<type_define> instance_;
	};
	std::unique_ptr<type_define> type_define::instance_;

	class shade_mode_define
	{
	public:
		static shade_mode_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<shade_mode_define>();
				}
			}
			return *instance_;
		}

		ShadeMode FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < sms_hash_.size(); ++ i)
			{
				if (sms_hash_[i] == name_hash)
				{
					return static_cast<ShadeMode>(i);
				}
			}

			KFL_UNREACHABLE("Invalid ShadeMode name");
		}

		shade_mode_define()
		{
			sms_hash_.push_back(CT_HASH("flat"));
			sms_hash_.push_back(CT_HASH("gouraud"));
		}

	private:
		std::vector<size_t> sms_hash_;

		static std::unique_ptr<shade_mode_define> instance_;
	};
	std::unique_ptr<shade_mode_define> shade_mode_define::instance_;

	class compare_function_define
	{
	public:
		static compare_function_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<compare_function_define>();
				}
			}
			return *instance_;
		}

		CompareFunction FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < cfs_hash_.size(); ++ i)
			{
				if (cfs_hash_[i] == name_hash)
				{
					return static_cast<CompareFunction>(i);
				}
			}

			KFL_UNREACHABLE("Invalid CompareFunction name");
		}

		compare_function_define()
		{
			cfs_hash_.push_back(CT_HASH("always_fail"));
			cfs_hash_.push_back(CT_HASH("always_pass"));
			cfs_hash_.push_back(CT_HASH("less"));
			cfs_hash_.push_back(CT_HASH("less_equal"));
			cfs_hash_.push_back(CT_HASH("equal"));
			cfs_hash_.push_back(CT_HASH("not_equal"));
			cfs_hash_.push_back(CT_HASH("greater_equal"));
			cfs_hash_.push_back(CT_HASH("greater"));
		}

	private:
		std::vector<size_t> cfs_hash_;

		static std::unique_ptr<compare_function_define> instance_;
	};
	std::unique_ptr<compare_function_define> compare_function_define::instance_;

	class cull_mode_define
	{
	public:
		static cull_mode_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<cull_mode_define>();
				}
			}
			return *instance_;
		}

		CullMode FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < cms_hash_.size(); ++ i)
			{
				if (cms_hash_[i] == name_hash)
				{
					return static_cast<CullMode>(i);
				}
			}

			KFL_UNREACHABLE("Invalid CullMode name");
		}

		cull_mode_define()
		{
			cms_hash_.push_back(CT_HASH("none"));
			cms_hash_.push_back(CT_HASH("front"));
			cms_hash_.push_back(CT_HASH("back"));
		}

	private:
		std::vector<size_t> cms_hash_;

		static std::unique_ptr<cull_mode_define> instance_;
	};
	std::unique_ptr<cull_mode_define> cull_mode_define::instance_;

	class polygon_mode_define
	{
	public:
		static polygon_mode_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<polygon_mode_define>();
				}
			}
			return *instance_;
		}

		PolygonMode FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < pms_hash_.size(); ++ i)
			{
				if (pms_hash_[i] == name_hash)
				{
					return static_cast<PolygonMode>(i);
				}
			}

			KFL_UNREACHABLE("Invalid PolygonMode name");
		}

		polygon_mode_define()
		{
			pms_hash_.push_back(CT_HASH("point"));
			pms_hash_.push_back(CT_HASH("line"));
			pms_hash_.push_back(CT_HASH("fill"));
		}

	private:
		std::vector<size_t> pms_hash_;

		static std::unique_ptr<polygon_mode_define> instance_;
	};
	std::unique_ptr<polygon_mode_define> polygon_mode_define::instance_;

	class alpha_blend_factor_define
	{
	public:
		static alpha_blend_factor_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<alpha_blend_factor_define>();
				}
			}
			return *instance_;
		}

		AlphaBlendFactor FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < abfs_hash_.size(); ++ i)
			{
				if (abfs_hash_[i] == name_hash)
				{
					return static_cast<AlphaBlendFactor>(i);
				}
			}

			KFL_UNREACHABLE("Invalid AlphaBlendFactor name");
		}

		alpha_blend_factor_define()
		{
			abfs_hash_.push_back(CT_HASH("zero"));
			abfs_hash_.push_back(CT_HASH("one"));
			abfs_hash_.push_back(CT_HASH("src_alpha"));
			abfs_hash_.push_back(CT_HASH("dst_alpha"));
			abfs_hash_.push_back(CT_HASH("inv_src_alpha"));
			abfs_hash_.push_back(CT_HASH("inv_dst_alpha"));
			abfs_hash_.push_back(CT_HASH("src_color"));
			abfs_hash_.push_back(CT_HASH("dst_color"));
			abfs_hash_.push_back(CT_HASH("inv_src_color"));
			abfs_hash_.push_back(CT_HASH("inv_dst_color"));
			abfs_hash_.push_back(CT_HASH("src_alpha_sat"));
			abfs_hash_.push_back(CT_HASH("blend_factor"));
			abfs_hash_.push_back(CT_HASH("inv_blend_factor"));
			abfs_hash_.push_back(CT_HASH("src1_alpha"));
			abfs_hash_.push_back(CT_HASH("inv_src1_alpha"));
			abfs_hash_.push_back(CT_HASH("src1_color"));
			abfs_hash_.push_back(CT_HASH("inv_src1_color"));
		}

	private:
		std::vector<size_t> abfs_hash_;

		static std::unique_ptr<alpha_blend_factor_define> instance_;
	};
	std::unique_ptr<alpha_blend_factor_define> alpha_blend_factor_define::instance_;

	class blend_operation_define
	{
	public:
		static blend_operation_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<blend_operation_define>();
				}
			}
			return *instance_;
		}

		BlendOperation FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < bops_hash_.size(); ++ i)
			{
				if (bops_hash_[i] == name_hash)
				{
					return static_cast<BlendOperation>(i + 1);
				}
			}

			KFL_UNREACHABLE("Invalid BlendOperation name");
		}

		blend_operation_define()
		{
			bops_hash_.push_back(CT_HASH("add"));
			bops_hash_.push_back(CT_HASH("sub"));
			bops_hash_.push_back(CT_HASH("rev_sub"));
			bops_hash_.push_back(CT_HASH("min"));
			bops_hash_.push_back(CT_HASH("max"));
		}

	private:
		std::vector<size_t> bops_hash_;

		static std::unique_ptr<blend_operation_define> instance_;
	};
	std::unique_ptr<blend_operation_define> blend_operation_define::instance_;

	class stencil_operation_define
	{
	public:
		static stencil_operation_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<stencil_operation_define>();
				}
			}
			return *instance_;
		}

		StencilOperation FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < sops_hash_.size(); ++ i)
			{
				if (sops_hash_[i] == name_hash)
				{
					return static_cast<StencilOperation>(i);
				}
			}

			KFL_UNREACHABLE("Invalid StencilOperation name");
		}

		stencil_operation_define()
		{
			sops_hash_.push_back(CT_HASH("keep"));
			sops_hash_.push_back(CT_HASH("zero"));
			sops_hash_.push_back(CT_HASH("replace"));
			sops_hash_.push_back(CT_HASH("incr"));
			sops_hash_.push_back(CT_HASH("decr"));
			sops_hash_.push_back(CT_HASH("invert"));
			sops_hash_.push_back(CT_HASH("incr_wrap"));
			sops_hash_.push_back(CT_HASH("decr_wrap"));
		}

	private:
		std::vector<size_t> sops_hash_;

		static std::unique_ptr<stencil_operation_define> instance_;
	};
	std::unique_ptr<stencil_operation_define> stencil_operation_define::instance_;

	class texture_filter_mode_define
	{
	public:
		static texture_filter_mode_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<texture_filter_mode_define>();
				}
			}
			return *instance_;
		}

		TexFilterOp FromStr(std::string_view name) const
		{
			int cmp;
			std::string_view f;
			if (0 == name.find("cmp_"))
			{
				cmp = 1;
				f = name.substr(4);
			}
			else
			{
				cmp = 0;
				f = name;
			}
			size_t const f_hash = HashRange(f.begin(), f.end());
			for (uint32_t i = 0; i < tfs_hash_.size(); ++ i)
			{
				if (tfs_hash_[i] == f_hash)
				{
					return static_cast<TexFilterOp>((cmp << 4) + i);
				}
			}
			if (CT_HASH("anisotropic") == f_hash)
			{
				return static_cast<TexFilterOp>((cmp << 4) + TFO_Anisotropic);
			}

			KFL_UNREACHABLE("Invalid TexFilterOp name");
		}

		texture_filter_mode_define()
		{
			tfs_hash_.push_back(CT_HASH("min_mag_mip_point"));
			tfs_hash_.push_back(CT_HASH("min_mag_point_mip_linear"));
			tfs_hash_.push_back(CT_HASH("min_point_mag_linear_mip_point"));
			tfs_hash_.push_back(CT_HASH("min_point_mag_mip_linear"));
			tfs_hash_.push_back(CT_HASH("min_linear_mag_mip_point"));
			tfs_hash_.push_back(CT_HASH("min_linear_mag_point_mip_linear"));
			tfs_hash_.push_back(CT_HASH("min_mag_linear_mip_point"));
			tfs_hash_.push_back(CT_HASH("min_mag_mip_linear"));
		}

	private:
		std::vector<size_t> tfs_hash_;

		static std::unique_ptr<texture_filter_mode_define> instance_;
	};
	std::unique_ptr<texture_filter_mode_define> texture_filter_mode_define::instance_;

	class texture_addr_mode_define
	{
	public:
		static texture_addr_mode_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<texture_addr_mode_define>();
				}
			}
			return *instance_;
		}

		TexAddressingMode FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < tams_hash_.size(); ++ i)
			{
				if (tams_hash_[i] == name_hash)
				{
					return static_cast<TexAddressingMode>(i);
				}
			}

			KFL_UNREACHABLE("Invalid TexAddressingMode name");
		}

		texture_addr_mode_define()
		{
			tams_hash_.push_back(CT_HASH("wrap"));
			tams_hash_.push_back(CT_HASH("mirror"));
			tams_hash_.push_back(CT_HASH("clamp"));
			tams_hash_.push_back(CT_HASH("border"));
		}

	private:
		std::vector<size_t> tams_hash_;

		static std::unique_ptr<texture_addr_mode_define> instance_;
	};
	std::unique_ptr<texture_addr_mode_define> texture_addr_mode_define::instance_;

	class logic_operation_define
	{
	public:
		static logic_operation_define& instance()
		{
			if (!instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex);
				if (!instance_)
				{
					instance_ = MakeUniquePtr<logic_operation_define>();
				}
			}
			return *instance_;
		}

		LogicOperation FromStr(std::string_view name) const
		{
			size_t const name_hash = HashRange(name.begin(), name.end());
			for (uint32_t i = 0; i < lops_hash_.size(); ++ i)
			{
				if (lops_hash_[i] == name_hash)
				{
					return static_cast<LogicOperation>(i);
				}
			}

			KFL_UNREACHABLE("Invalid LogicOperation name");
		}

		logic_operation_define()
		{
			lops_hash_.push_back(CT_HASH("clear"));
			lops_hash_.push_back(CT_HASH("set"));
			lops_hash_.push_back(CT_HASH("copy"));
			lops_hash_.push_back(CT_HASH("copy_inverted"));
			lops_hash_.push_back(CT_HASH("noop"));
			lops_hash_.push_back(CT_HASH("invert"));
			lops_hash_.push_back(CT_HASH("and"));
			lops_hash_.push_back(CT_HASH("nand"));
			lops_hash_.push_back(CT_HASH("or"));
			lops_hash_.push_back(CT_HASH("nor"));
			lops_hash_.push_back(CT_HASH("xor"));
			lops_hash_.push_back(CT_HASH("equiv"));
			lops_hash_.push_back(CT_HASH("and_reverse"));
			lops_hash_.push_back(CT_HASH("and_inverted"));
			lops_hash_.push_back(CT_HASH("or_reverse"));
			lops_hash_.push_back(CT_HASH("or_inverted"));
		}

	private:
		std::vector<size_t> lops_hash_;

		static std::unique_ptr<logic_operation_define> instance_;
	};
	std::unique_ptr<logic_operation_define> logic_operation_define::instance_;

#if KLAYGE_IS_DEV_PLATFORM
	bool BoolFromStr(std::string_view name)
	{
		if (("true" == name) || ("1" == name))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	int get_index(XMLNodePtr const & node)
	{
		int index = 0;
		XMLAttributePtr attr = node->Attrib("index");
		if (attr)
		{
			index = attr->ValueInt();
		}
		return index;
	}

	std::string get_profile(XMLNodePtr const & node)
	{
		XMLAttributePtr attr = node->Attrib("profile");
		if (attr)
		{
			return attr->ValueString();
		}
		else
		{
			return "auto";
		}
	}

	std::string get_func_name(XMLNodePtr const & node)
	{
		std::string value = node->Attrib("value")->ValueString();
		return value.substr(0, value.find("("));
	}

	std::unique_ptr<RenderVariable> read_var(XMLNodePtr const & node, uint32_t type, uint32_t array_size)
	{
		std::unique_ptr<RenderVariable> var;
		XMLAttributePtr attr;

		switch (type)
		{
		case REDT_bool:
			if (0 == array_size)
			{
				attr = node->Attrib("value");
				bool tmp = false;
				if (attr)
				{
					std::string value_str = attr->ValueString();
					tmp = BoolFromStr(value_str);
				}

				var = MakeUniquePtr<RenderVariableBool>();
				*var = tmp;
			}
			break;

		case REDT_uint:
			if (0 == array_size)
			{
				attr = node->Attrib("value");
				uint32_t tmp = 0;
				if (attr)
				{
					tmp = attr->ValueInt();
				}

				var = MakeUniquePtr<RenderVariableUInt>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableUIntArray>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<uint32_t> init_val(std::min(array_size, static_cast<uint32_t>(strs.size())), 0);
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							if (index < strs.size())
							{
								boost::algorithm::trim(strs[index]);
								init_val[index] = boost::lexical_cast<uint32_t>(strs[index]);
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_int:
			if (0 == array_size)
			{
				attr = node->Attrib("value");
				int32_t tmp = 0;
				if (attr)
				{
					tmp = attr->ValueInt();
				}

				var = MakeUniquePtr<RenderVariableInt>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableIntArray>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<int32_t> init_val(std::min(array_size, static_cast<uint32_t>(strs.size())), 0);
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							if (index < strs.size())
							{
								boost::algorithm::trim(strs[index]);
								init_val[index] = boost::lexical_cast<int32_t>(strs[index]);
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_string:
			{
				attr = node->Attrib("value");
				std::string tmp;
				if (attr)
				{
					tmp = attr->ValueString();
				}

				var = MakeUniquePtr<RenderVariableString>();
				*var = tmp;
			}
			break;

		case REDT_texture1D:
		case REDT_texture2D:
		case REDT_texture3D:
		case REDT_textureCUBE:
		case REDT_texture1DArray:
		case REDT_texture2DArray:
		case REDT_texture3DArray:
		case REDT_textureCUBEArray:
		case REDT_rw_texture1D:
		case REDT_rw_texture2D:
		case REDT_rw_texture3D:
		case REDT_rw_texture1DArray:
		case REDT_rw_texture2DArray:
			var = MakeUniquePtr<RenderVariableTexture>();
			*var = TexturePtr();
			attr = node->Attrib("elem_type");
			if (attr)
			{
				*var = attr->ValueString();
			}
			else
			{
				*var = std::string("float4");
			}
			break;

		case REDT_sampler:
			{
				SamplerStateDesc desc;

				for (XMLNodePtr state_node = node->FirstNode("state"); state_node; state_node = state_node->NextSibling("state"))
				{
					std::string name = state_node->Attrib("name")->ValueString();
					size_t const name_hash = RT_HASH(name.c_str());

					if (CT_HASH("filtering") == name_hash)
					{
						std::string value_str = state_node->Attrib("value")->ValueString();
						desc.filter = texture_filter_mode_define::instance().FromStr(value_str);
					}
					else if (CT_HASH("address_u") == name_hash)
					{
						std::string value_str = state_node->Attrib("value")->ValueString();
						desc.addr_mode_u = texture_addr_mode_define::instance().FromStr(value_str);
					}
					else if (CT_HASH("address_v") == name_hash)
					{
						std::string value_str = state_node->Attrib("value")->ValueString();
						desc.addr_mode_v = texture_addr_mode_define::instance().FromStr(value_str);
					}
					else if (CT_HASH("address_w") == name_hash)
					{
						std::string value_str = state_node->Attrib("value")->ValueString();
						desc.addr_mode_w = texture_addr_mode_define::instance().FromStr(value_str);
					}
					else if (CT_HASH("max_anisotropy") == name_hash)
					{
						desc.max_anisotropy = static_cast<uint8_t>(state_node->Attrib("value")->ValueUInt());
					}
					else if (CT_HASH("min_lod") == name_hash)
					{
						desc.min_lod = state_node->Attrib("value")->ValueFloat();
					}
					else if (CT_HASH("max_lod") == name_hash)
					{
						desc.max_lod = state_node->Attrib("value")->ValueFloat();
					}
					else if (CT_HASH("mip_map_lod_bias") == name_hash)
					{
						desc.mip_map_lod_bias = state_node->Attrib("value")->ValueFloat();
					}
					else if (CT_HASH("cmp_func") == name_hash)
					{
						std::string value_str = state_node->Attrib("value")->ValueString();
						desc.cmp_func = compare_function_define::instance().FromStr(value_str);
					}
					else if (CT_HASH("border_clr") == name_hash)
					{
						attr = state_node->Attrib("r");
						if (attr)
						{
							desc.border_clr.r() = attr->ValueFloat();
						}
						attr = state_node->Attrib("g");
						if (attr)
						{
							desc.border_clr.g() = attr->ValueFloat();
						}
						attr = state_node->Attrib("b");
						if (attr)
						{
							desc.border_clr.b() = attr->ValueFloat();
						}
						attr = state_node->Attrib("a");
						if (attr)
						{
							desc.border_clr.a() = attr->ValueFloat();
						}
					}
					else
					{
						KFL_UNREACHABLE("Invalid sampler state name");
					}
				}

				var = MakeUniquePtr<RenderVariableSampler>();
				*var = Context::Instance().RenderFactoryInstance().MakeSamplerStateObject(desc);
			}
			break;

		case REDT_shader:
			{
				ShaderDesc desc;
				desc.profile = get_profile(node);
				desc.func_name = get_func_name(node);

				var = MakeUniquePtr<RenderVariableShader>();
				*var = desc;
			}
			break;

		case REDT_float:
			if (0 == array_size)
			{
				float tmp = 0;
				attr = node->Attrib("value");
				if (attr)
				{
					tmp = attr->ValueFloat();
				}

				var = MakeUniquePtr<RenderVariableFloat>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloatArray>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<float> init_val(std::min(array_size, static_cast<uint32_t>(strs.size())), 0.0f);
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							if (index < strs.size())
							{
								boost::algorithm::trim(strs[index]);
								init_val[index] = boost::lexical_cast<float>(strs[index]);
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_uint2:
			if (0 == array_size)
			{
				uint2 tmp(0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueUInt();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueUInt();
				}

				var = MakeUniquePtr<RenderVariableUInt2>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt2Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<uint2> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 1) / 2)), int2(0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 2; ++ j)
							{
								if (index * 2 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 2 + j]);
									init_val[index][j] = boost::lexical_cast<uint32_t>(strs[index * 2 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_uint3:
			if (0 == array_size)
			{
				uint3 tmp(0, 0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueUInt();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueUInt();
				}
				attr = node->Attrib("z");
				if (attr)
				{
					tmp.z() = attr->ValueUInt();
				}

				var = MakeUniquePtr<RenderVariableUInt3>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt3Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<uint3> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 2) / 3)), int3(0, 0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 3; ++ j)
							{
								if (index * 3 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 3 + j]);
									init_val[index][j] = boost::lexical_cast<uint32_t>(strs[index * 3 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_uint4:
			if (0 == array_size)
			{
				uint4 tmp(0, 0, 0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueUInt();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueUInt();
				}
				attr = node->Attrib("z");
				if (attr)
				{
					tmp.z() = attr->ValueUInt();
				}
				attr = node->Attrib("w");
				if (attr)
				{
					tmp.w() = attr->ValueUInt();
				}

				var = MakeUniquePtr<RenderVariableUInt4>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt4Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<int4> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 3) / 4)), int4(0, 0, 0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 4; ++ j)
							{
								if (index * 4 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 4 + j]);
									init_val[index][j] = boost::lexical_cast<uint32_t>(strs[index * 4 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_int2:
			if (0 == array_size)
			{
				int2 tmp(0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueInt();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueInt();
				}

				var = MakeUniquePtr<RenderVariableInt2>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt2Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<int2> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 1) / 2)), int2(0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 2; ++ j)
							{
								if (index * 2 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 2 + j]);
									init_val[index][j] = boost::lexical_cast<int32_t>(strs[index * 2 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_int3:
			if (0 == array_size)
			{
				int3 tmp(0, 0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueInt();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueInt();
				}
				attr = node->Attrib("z");
				if (attr)
				{
					tmp.z() = attr->ValueInt();
				}

				var = MakeUniquePtr<RenderVariableInt3>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt3Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<int3> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 2) / 3)), int3(0, 0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 3; ++ j)
							{
								if (index * 3 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 3 + j]);
									init_val[index][j] = boost::lexical_cast<int32_t>(strs[index * 3 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_int4:
			if (0 == array_size)
			{
				int4 tmp(0, 0, 0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueInt();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueInt();
				}
				attr = node->Attrib("z");
				if (attr)
				{
					tmp.z() = attr->ValueInt();
				}
				attr = node->Attrib("w");
				if (attr)
				{
					tmp.w() = attr->ValueInt();
				}

				var = MakeUniquePtr<RenderVariableInt4>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt4Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<int4> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 3) / 4)), int4(0, 0, 0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 4; ++ j)
							{
								if (index * 4 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 4 + j]);
									init_val[index][j] = boost::lexical_cast<int32_t>(strs[index * 4 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_float2:
			if (0 == array_size)
			{
				float2 tmp(0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueFloat();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueFloat();
				}

				var = MakeUniquePtr<RenderVariableFloat2>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloat2Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<float2> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 1) / 2)), float2(0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 2; ++ j)
							{
								if (index * 2 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 2 + j]);
									init_val[index][j] = boost::lexical_cast<float>(strs[index * 2 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_float3:
			if (0 == array_size)
			{
				float3 tmp(0, 0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueFloat();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueFloat();
				}
				attr = node->Attrib("z");
				if (attr)
				{
					tmp.z() = attr->ValueFloat();
				}

				var = MakeUniquePtr<RenderVariableFloat3>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloat3Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<float3> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 2) / 3)), float3(0, 0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 3; ++ j)
							{
								if (index * 3 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 3 + j]);
									init_val[index][j] = boost::lexical_cast<float>(strs[index * 3 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_float4:
			if (0 == array_size)
			{
				float4 tmp(0, 0, 0, 0);
				attr = node->Attrib("x");
				if (attr)
				{
					tmp.x() = attr->ValueFloat();
				}
				attr = node->Attrib("y");
				if (attr)
				{
					tmp.y() = attr->ValueFloat();
				}
				attr = node->Attrib("z");
				if (attr)
				{
					tmp.z() = attr->ValueFloat();
				}
				attr = node->Attrib("w");
				if (attr)
				{
					tmp.w() = attr->ValueFloat();
				}

				var = MakeUniquePtr<RenderVariableFloat4>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloat4Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<float4> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 3) / 4)), float4(0, 0, 0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 4; ++ j)
							{
								if (index * 4 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 4 + j]);
									init_val[index][j] = boost::lexical_cast<float>(strs[index * 4 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_float4x4:
			if (0 == array_size)
			{
				float4x4 tmp(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
				for (int y = 0; y < 4; ++ y)
				{
					for (int x = 0; x < 4; ++ x)
					{
						attr = node->Attrib(std::string("_")
							+ static_cast<char>('0' + y) + static_cast<char>('0' + x));
						if (attr)
						{
							tmp[y * 4 + x] = attr->ValueFloat();
						}
					}
				}

				var = MakeUniquePtr<RenderVariableFloat4x4>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloat4x4Array>();

				XMLNodePtr value_node = node->FirstNode("value");
				if (value_node)
				{
					value_node = value_node->FirstNode();
					if (value_node && (XNT_CData == value_node->Type()))
					{
						std::string value_str = value_node->ValueString();
						std::vector<std::string> strs;
						boost::algorithm::split(strs, value_str, boost::is_any_of(","));
						std::vector<float4> init_val(std::min(array_size, static_cast<uint32_t>((strs.size() + 3) / 4)), float4(0, 0, 0, 0));
						for (size_t index = 0; index < init_val.size(); ++ index)
						{
							for (size_t j = 0; j < 16; ++ j)
							{
								if (index * 16 + j < strs.size())
								{
									boost::algorithm::trim(strs[index * 16 + j]);
									init_val[index][j] = boost::lexical_cast<float>(strs[index * 16 + j]);
								}
							}
						}
						*var = init_val;
					}
				}
			}
			break;

		case REDT_buffer:
		case REDT_structured_buffer:
		case REDT_rw_buffer:
		case REDT_rw_structured_buffer:
		case REDT_consume_structured_buffer:
		case REDT_append_structured_buffer:
			var = MakeUniquePtr<RenderVariableBuffer>();
			*var = GraphicsBufferPtr();
			attr = node->Attrib("elem_type");
			if (attr)
			{
				*var = attr->ValueString();
			}
			else
			{
				*var = std::string("float4");
			}
			break;

		case REDT_byte_address_buffer:
		case REDT_rw_byte_address_buffer:
			var = MakeUniquePtr<RenderVariableByteAddressBuffer>();
			*var = GraphicsBufferPtr();
			break;

		default:
			KFL_UNREACHABLE("Invalid type");
		}

		return var;
	}
#endif

	std::unique_ptr<RenderVariable> stream_in_var(ResIdentifierPtr const & res, uint32_t type, uint32_t array_size)
	{
		std::unique_ptr<RenderVariable> var;

		switch (type)
		{
		case REDT_bool:
			if (0 == array_size)
			{
				bool tmp;
				res->read(&tmp, sizeof(tmp));

				var = MakeUniquePtr<RenderVariableBool>();
				*var = tmp;
			}
			break;

		case REDT_uint:
			if (0 == array_size)
			{
				uint32_t tmp;
				res->read(&tmp, sizeof(tmp));

				var = MakeUniquePtr<RenderVariableUInt>();
				*var = LE2Native(tmp);
			}
			else
			{
				var = MakeUniquePtr<RenderVariableUIntArray>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<uint32_t> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						init_val[i] = LE2Native(init_val[i]);
					}
					*var = init_val;
				}
			}
			break;

		case REDT_int:
			if (0 == array_size)
			{
				int32_t tmp;
				res->read(&tmp, sizeof(tmp));

				var = MakeUniquePtr<RenderVariableInt>();
				*var = LE2Native(tmp);
			}
			else
			{
				var = MakeUniquePtr<RenderVariableIntArray>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<int32_t> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						init_val[i] = LE2Native(init_val[i]);
					}
					*var = init_val;
				}
			}
			break;

		case REDT_string:
			{
				var = MakeUniquePtr<RenderVariableString>();
				*var = ReadShortString(res);
			}
			break;

		case REDT_texture1D:
		case REDT_texture2D:
		case REDT_texture3D:
		case REDT_textureCUBE:
		case REDT_texture1DArray:
		case REDT_texture2DArray:
		case REDT_texture3DArray:
		case REDT_textureCUBEArray:
		case REDT_rw_texture1D:
		case REDT_rw_texture2D:
		case REDT_rw_texture3D:
		case REDT_rw_texture1DArray:
		case REDT_rw_texture2DArray:
			{
				var = MakeUniquePtr<RenderVariableTexture>();
				*var = TexturePtr();
				*var = ReadShortString(res);
			}
			break;

		case REDT_sampler:
			{
				SamplerStateDesc desc;
				res->read(&desc, sizeof(desc));
				desc.border_clr[0] = LE2Native(desc.border_clr[0]);
				desc.border_clr[1] = LE2Native(desc.border_clr[1]);
				desc.border_clr[2] = LE2Native(desc.border_clr[2]);
				desc.border_clr[3] = LE2Native(desc.border_clr[3]);
				desc.addr_mode_u = LE2Native(desc.addr_mode_u);
				desc.addr_mode_v = LE2Native(desc.addr_mode_v);
				desc.addr_mode_w = LE2Native(desc.addr_mode_w);
				desc.filter = LE2Native(desc.filter);
				desc.min_lod = LE2Native(desc.min_lod);
				desc.max_lod = LE2Native(desc.max_lod);
				desc.mip_map_lod_bias = LE2Native(desc.mip_map_lod_bias);
				desc.cmp_func = LE2Native(desc.cmp_func);

				var = MakeUniquePtr<RenderVariableSampler>();
				*var = Context::Instance().RenderFactoryInstance().MakeSamplerStateObject(desc);
			}
			break;

		case REDT_shader:
			{
				ShaderDesc desc;
				desc.profile = ReadShortString(res);
				desc.func_name = ReadShortString(res);

				var = MakeUniquePtr<RenderVariableShader>();
				*var = desc;
			}
			break;

		case REDT_float:
			if (0 == array_size)
			{
				float tmp;
				res->read(&tmp, sizeof(tmp));

				var = MakeUniquePtr<RenderVariableFloat>();
				*var = LE2Native(tmp);
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloatArray>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<float> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						init_val[i] = LE2Native(init_val[i]);
					}
					*var = init_val;
				}
			}
			break;

		case REDT_uint2:
			if (0 == array_size)
			{
				uint2 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 2; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableUInt2>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt2Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<int2> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 2; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_uint3:
			if (0 == array_size)
			{
				uint3 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 3; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableUInt3>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt3Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<int3> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 3; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_uint4:
			if (0 == array_size)
			{
				uint4 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 4; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableUInt4>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt4Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<int4> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 4; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_int2:
			if (0 == array_size)
			{
				int2 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 2; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableInt2>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt2Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<int2> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 2; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_int3:
			if (0 == array_size)
			{
				int3 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 3; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableInt3>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt3Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<int3> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 3; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_int4:
			if (0 == array_size)
			{
				int4 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 4; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableInt4>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableInt4Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<int4> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 4; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_float2:
			if (0 == array_size)
			{
				float2 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 2; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableFloat2>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloat2Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<float2> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 2; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_float3:
			if (0 == array_size)
			{
				float3 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 3; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableFloat3>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloat3Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<float3> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 3; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_float4:
			if (0 == array_size)
			{
				float4 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 4; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableFloat4>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloat4Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<float4> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 4; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_float4x4:
			if (0 == array_size)
			{
				float4x4 tmp;
				res->read(&tmp, sizeof(tmp));
				for (int i = 0; i < 16; ++ i)
				{
					tmp[i] = LE2Native(tmp[i]);
				}

				var = MakeUniquePtr<RenderVariableFloat4x4>();
				*var = tmp;
			}
			else
			{
				var = MakeUniquePtr<RenderVariableFloat4x4Array>();

				uint32_t len;
				res->read(&len, sizeof(len));
				len = LE2Native(len);
				if (len > 0)
				{
					std::vector<float4x4> init_val(len);
					res->read(&init_val[0], len * sizeof(init_val[0]));
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 16; ++ j)
						{
							init_val[i][j] = LE2Native(init_val[i][j]);
						}
					}
					*var = init_val;
				}
			}
			break;

		case REDT_buffer:
		case REDT_structured_buffer:
		case REDT_rw_buffer:
		case REDT_rw_structured_buffer:
		case REDT_consume_structured_buffer:
		case REDT_append_structured_buffer:
			{
				var = MakeUniquePtr<RenderVariableBuffer>();
				*var = GraphicsBufferPtr();
				*var = ReadShortString(res);
			}
			break;

		case REDT_byte_address_buffer:
		case REDT_rw_byte_address_buffer:
			var = MakeUniquePtr<RenderVariableByteAddressBuffer>();
			*var = GraphicsBufferPtr();
			break;

		default:
			KFL_UNREACHABLE("Invalid type");
		}

		return var;
	}

#if KLAYGE_IS_DEV_PLATFORM
	void stream_out_var(std::ostream& os, RenderVariable const & var, uint32_t type, uint32_t array_size)
	{
		switch (type)
		{
		case REDT_bool:
			if (0 == array_size)
			{
				bool tmp;
				var.Value(tmp);

				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			break;

		case REDT_uint:
			if (0 == array_size)
			{
				uint32_t tmp;
				var.Value(tmp);

				tmp = Native2LE(tmp);
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<uint32_t> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						init_val[i] = Native2LE(init_val[i]);
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_int:
			if (0 == array_size)
			{
				int32_t tmp;
				var.Value(tmp);

				tmp = Native2LE(tmp);
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<int32_t> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						init_val[i] = Native2LE(init_val[i]);
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_string:
			{
				std::string tmp;
				var.Value(tmp);
				WriteShortString(os, tmp);
			}
			break;

		case REDT_texture1D:
		case REDT_texture2D:
		case REDT_texture3D:
		case REDT_textureCUBE:
		case REDT_texture1DArray:
		case REDT_texture2DArray:
		case REDT_texture3DArray:
		case REDT_textureCUBEArray:
		case REDT_rw_texture1D:
		case REDT_rw_texture2D:
		case REDT_rw_texture3D:
		case REDT_rw_texture1DArray:
		case REDT_rw_texture2DArray:
			{
				std::string tmp;
				var.Value(tmp);
				WriteShortString(os, tmp);
			}
			break;

		case REDT_sampler:
			{
				SamplerStateObjectPtr tmp;
				var.Value(tmp);
				SamplerStateDesc desc = tmp->GetDesc();
				desc.border_clr[0] = Native2LE(desc.border_clr[0]);
				desc.border_clr[1] = Native2LE(desc.border_clr[1]);
				desc.border_clr[2] = Native2LE(desc.border_clr[2]);
				desc.border_clr[3] = Native2LE(desc.border_clr[3]);
				desc.addr_mode_u = Native2LE(desc.addr_mode_u);
				desc.addr_mode_v = Native2LE(desc.addr_mode_v);
				desc.addr_mode_w = Native2LE(desc.addr_mode_w);
				desc.filter = Native2LE(desc.filter);
				desc.min_lod = Native2LE(desc.min_lod);
				desc.max_lod = Native2LE(desc.max_lod);
				desc.mip_map_lod_bias = Native2LE(desc.mip_map_lod_bias);
				desc.cmp_func = Native2LE(desc.cmp_func);
				os.write(reinterpret_cast<char const *>(&desc), sizeof(desc));
			}
			break;

		case REDT_shader:
			{
				ShaderDesc tmp;
				var.Value(tmp);
				WriteShortString(os, tmp.profile);
				WriteShortString(os, tmp.func_name);
			}
			break;

		case REDT_float:
			if (0 == array_size)
			{
				float tmp;
				var.Value(tmp);

				tmp = Native2LE(tmp);
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<float> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						init_val[i] = Native2LE(init_val[i]);
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_uint2:
			if (0 == array_size)
			{
				uint2 tmp;
				var.Value(tmp);

				for (int i = 0; i < 2; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<int2> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 2; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_uint3:
			if (0 == array_size)
			{
				uint3 tmp;
				var.Value(tmp);

				for (int i = 0; i < 3; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<int3> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 3; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_uint4:
			if (0 == array_size)
			{
				uint4 tmp;
				var.Value(tmp);

				for (int i = 0; i < 3; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<int4> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 4; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_int2:
			if (0 == array_size)
			{
				int2 tmp;
				var.Value(tmp);

				for (int i = 0; i < 2; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<int2> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 2; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_int3:
			if (0 == array_size)
			{
				int3 tmp;
				var.Value(tmp);

				for (int i = 0; i < 3; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<int3> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 3; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_int4:
			if (0 == array_size)
			{
				int4 tmp;
				var.Value(tmp);

				for (int i = 0; i < 4; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<int4> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 4; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_float2:
			if (0 == array_size)
			{
				float2 tmp;
				var.Value(tmp);

				for (int i = 0; i < 2; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<float2> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 2; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_float3:
			if (0 == array_size)
			{
				float3 tmp;
				var.Value(tmp);

				for (int i = 0; i < 3; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<float3> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 3; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char*>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_float4:
			if (0 == array_size)
			{
				float4 tmp;
				var.Value(tmp);

				for (int i = 0; i < 4; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<float4> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 4; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_float4x4:
			if (0 == array_size)
			{
				float4x4 tmp;
				var.Value(tmp);

				for (int i = 0; i < 16; ++ i)
				{
					tmp[i] = Native2LE(tmp[i]);
				}
				os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
			}
			else
			{
				std::vector<float4x4> init_val;
				var.Value(init_val);

				uint32_t len = static_cast<uint32_t>(init_val.size());
				len = Native2LE(len);
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				if (len > 0)
				{
					for (uint32_t i = 0; i < len; ++ i)
					{
						for (int j = 0; j < 16; ++ j)
						{
							init_val[i][j] = Native2LE(init_val[i][j]);
						}
					}
					os.write(reinterpret_cast<char const *>(&init_val[0]), len * sizeof(init_val[0]));
				}
			}
			break;

		case REDT_buffer:
		case REDT_structured_buffer:
		case REDT_rw_buffer:
		case REDT_rw_structured_buffer:
		case REDT_consume_structured_buffer:
		case REDT_append_structured_buffer:
			{
				std::string tmp;
				var.Value(tmp);
				WriteShortString(os, tmp);
			}
			break;

		case REDT_byte_address_buffer:
		case REDT_rw_byte_address_buffer:
			break;

		default:
			KFL_UNREACHABLE("Invalid type");
		}
	}
#endif
}

namespace KlayGE
{
	class EffectLoadingDesc : public ResLoadingDesc
	{
	private:
		struct EffectDesc
		{
			std::string res_name;

			RenderEffectPtr effect;
		};

	public:
		explicit EffectLoadingDesc(std::string const & name)
		{
			effect_desc_.res_name = name;
		}

		uint64_t Type() const override
		{
			static uint64_t const type = CT_HASH("EffectLoadingDesc");
			return type;
		}

		bool StateLess() const override
		{
			return false;
		}

		void SubThreadStage() override
		{
		}

		void MainThreadStage() override
		{
			effect_desc_.effect = MakeSharedPtr<RenderEffect>();
			effect_desc_.effect->Load(effect_desc_.res_name);
		}

		bool HasSubThreadStage() const override
		{
			return false;
		}

		bool Match(ResLoadingDesc const & rhs) const override
		{
			if (this->Type() == rhs.Type())
			{
				EffectLoadingDesc const & eld = static_cast<EffectLoadingDesc const &>(rhs);
				return (effect_desc_.res_name == eld.effect_desc_.res_name);
			}
			return false;
		}

		void CopyDataFrom(ResLoadingDesc const & rhs) override
		{
			BOOST_ASSERT(this->Type() == rhs.Type());

			EffectLoadingDesc const & eld = static_cast<EffectLoadingDesc const &>(rhs);
			effect_desc_.res_name = eld.effect_desc_.res_name;
		}

		std::shared_ptr<void> CloneResourceFrom(std::shared_ptr<void> const & resource) override
		{
			effect_desc_.effect = std::static_pointer_cast<RenderEffect>(resource)->Clone();
			return std::static_pointer_cast<void>(effect_desc_.effect);
		}

		std::shared_ptr<void> Resource() const override
		{
			return effect_desc_.effect;
		}

	private:
		EffectDesc effect_desc_;
	};


#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectAnnotation::Load(XMLNodePtr const & node)
	{
		type_ = type_define::instance().TypeCode(node->Attrib("type")->ValueString());
		name_ = node->Attrib("name")->ValueString();
		var_ = read_var(node, type_, 0);
	}
#endif

	void RenderEffectAnnotation::StreamIn(ResIdentifierPtr const & res)
	{
		res->read(&type_, sizeof(type_));
		type_ = LE2Native(type_);
		name_ = ReadShortString(res);
		var_ = stream_in_var(res, type_, 0);
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectAnnotation::StreamOut(std::ostream& os) const
	{
		uint32_t t = Native2LE(type_);
		os.write(reinterpret_cast<char const *>(&t), sizeof(t));
		WriteShortString(os, name_);
		stream_out_var(os, *var_, type_, 0);
	}
#endif


	void RenderEffect::Load(std::string const & name)
	{
		effect_template_ = MakeSharedPtr<RenderEffectTemplate>();
		effect_template_->Load(name, *this);
	}

	RenderEffectPtr RenderEffect::Clone()
	{
		RenderEffectPtr ret = MakeSharedPtr<RenderEffect>();

		ret->effect_template_ = effect_template_;

		ret->params_.resize(params_.size());
		for (size_t i = 0; i < params_.size(); ++ i)
		{
			ret->params_[i] = params_[i]->Clone();
		}

		ret->cbuffers_.resize(cbuffers_.size());
		for (size_t i = 0; i < cbuffers_.size(); ++ i)
		{
			ret->cbuffers_[i] = cbuffers_[i]->Clone(*this, *ret);
		}

		ret->shader_objs_.resize(shader_objs_.size());
		for (size_t i = 0; i < shader_objs_.size(); ++ i)
		{
			ret->shader_objs_[i] = shader_objs_[i]->Clone(*ret);
		}

		return ret;
	}

	std::string const & RenderEffect::ResName() const
	{
		return effect_template_->ResName();
	}

	size_t RenderEffect::ResNameHash() const
	{
		return effect_template_->ResNameHash();
	}

	RenderEffectParameter* RenderEffect::ParameterByName(std::string_view name) const
	{
		size_t const name_hash = HashRange(name.begin(), name.end());
		for (auto const & param : params_)
		{
			if (name_hash == param->NameHash())
			{
				return param.get();
			}
		}
		return nullptr;
	}

	RenderEffectParameter* RenderEffect::ParameterBySemantic(std::string_view semantic) const
	{
		size_t const semantic_hash = HashRange(semantic.begin(), semantic.end());
		for (auto const & param : params_)
		{
			if (semantic_hash == param->SemanticHash())
			{
				return param.get();
			}
		}
		return nullptr;
	}

	RenderEffectConstantBuffer* RenderEffect::CBufferByName(std::string_view name) const
	{
		size_t const name_hash = HashRange(name.begin(), name.end());
		for (auto const & cbuffer : cbuffers_)
		{
			if (name_hash == cbuffer->NameHash())
			{
				return cbuffer.get();
			}
		}
		return nullptr;
	}

	uint32_t RenderEffect::NumTechniques() const
	{
		return effect_template_->NumTechniques();
	}

	RenderTechnique* RenderEffect::TechniqueByName(std::string_view name) const
	{
		return effect_template_->TechniqueByName(name);
	}

	RenderTechnique* RenderEffect::TechniqueByIndex(uint32_t n) const
	{
		return effect_template_->TechniqueByIndex(n);
	}

	uint32_t RenderEffect::NumShaderFragments() const
	{
		return effect_template_->NumShaderFragments();
	}

	RenderShaderFragment const & RenderEffect::ShaderFragmentByIndex(uint32_t n) const
	{
		return effect_template_->ShaderFragmentByIndex(n);
	}

	uint32_t RenderEffect::AddShaderDesc(ShaderDesc const & sd)
	{
		return effect_template_->AddShaderDesc(sd);
	}

	ShaderDesc& RenderEffect::GetShaderDesc(uint32_t id)
	{
		return effect_template_->GetShaderDesc(id);
	}

	ShaderDesc const & RenderEffect::GetShaderDesc(uint32_t id) const
	{
		return effect_template_->GetShaderDesc(id);
	}

	uint32_t RenderEffect::NumMacros() const
	{
		return effect_template_->NumMacros();
	}

	std::pair<std::string, std::string> const & RenderEffect::MacroByIndex(uint32_t n) const
	{
		return effect_template_->MacroByIndex(n);
	}

	uint32_t RenderEffect::AddShaderObject()
	{
		uint32_t index = static_cast<uint32_t>(shader_objs_.size());
		shader_objs_.push_back(Context::Instance().RenderFactoryInstance().MakeShaderObject());
		return index;
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffect::GenHLSLShaderText()
	{
		effect_template_->GenHLSLShaderText(*this);
	}

	std::string const & RenderEffect::HLSLShaderText() const
	{
		return effect_template_->HLSLShaderText();
	}
#endif


#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectTemplate::RecursiveIncludeNode(XMLNode const & root, std::vector<std::string>& include_names) const
	{
		for (XMLNodePtr node = root.FirstNode("include"); node; node = node->NextSibling("include"))
		{
			XMLAttributePtr attr = node->Attrib("name");
			BOOST_ASSERT(attr);

			std::string include_name = attr->ValueString();

			XMLDocument include_doc;
			XMLNodePtr include_root = include_doc.Parse(ResLoader::Instance().Open(include_name));
			this->RecursiveIncludeNode(*include_root, include_names);

			bool found = false;
			for (size_t i = 0; i < include_names.size(); ++ i)
			{
				if (include_name == include_names[i])
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				include_names.push_back(include_name);
			}
		}
	}

	void RenderEffectTemplate::InsertIncludeNodes(XMLDocument& target_doc, XMLNode& target_root,
		XMLNodePtr const & target_place, XMLNode const & include_root) const
	{
		for (XMLNodePtr child_node = include_root.FirstNode(); child_node; child_node = child_node->NextSibling())
		{
			if ((XNT_Element == child_node->Type()) && (child_node->Name() != "include"))
			{
				target_root.InsertNode(target_place, target_doc.CloneNode(child_node));
			}
		}
	}
#endif

	void RenderEffectTemplate::Load(std::string const & name, RenderEffect& effect)
	{
		std::string fxml_name = ResLoader::Instance().Locate(name);
		if (fxml_name.empty())
		{
			fxml_name = name;
		}
		std::string kfx_name = fxml_name.substr(0, fxml_name.rfind(".")) + ".kfx";

#if KLAYGE_IS_DEV_PLATFORM
		ResIdentifierPtr source = ResLoader::Instance().Open(fxml_name);
#endif
		ResIdentifierPtr kfx_source = ResLoader::Instance().Open(kfx_name);

#if KLAYGE_IS_DEV_PLATFORM
		std::unique_ptr<XMLDocument> doc;
		XMLNodePtr root;
#endif

		res_name_ = fxml_name;
		res_name_hash_ = HashRange(fxml_name.begin(), fxml_name.end());
#if KLAYGE_IS_DEV_PLATFORM
		if (source)
		{
			timestamp_ = source->Timestamp();

			doc = MakeUniquePtr<XMLDocument>();
			root = doc->Parse(source);

			std::vector<std::string> include_names;
			this->RecursiveIncludeNode(*root, include_names);

			for (auto const & include_name : include_names)
			{
				ResIdentifierPtr include_source = ResLoader::Instance().Open(include_name);
				if (include_source)
				{
					timestamp_ = std::max(timestamp_, include_source->Timestamp());
				}
			}
		}
		else
		{
			timestamp_ = 0;
		}
#endif

		if (!this->StreamIn(kfx_source, effect))
		{
#if KLAYGE_IS_DEV_PLATFORM
			if (source)
			{
				effect.params_.clear();
				effect.cbuffers_.clear();
				effect.shader_objs_.clear();

				macros_.reset();
				shader_frags_.clear();
				hlsl_shader_.clear();
				techniques_.clear();

				shader_descs_.resize(1);

				XMLAttributePtr attr;

				std::vector<std::unique_ptr<XMLDocument>> include_docs;
				std::vector<std::string> whole_include_names;
				for (XMLNodePtr node = root->FirstNode("include"); node;)
				{
					attr = node->Attrib("name");
					BOOST_ASSERT(attr);
					std::string include_name = attr->ValueString();

					include_docs.push_back(MakeUniquePtr<XMLDocument>());
					XMLNodePtr include_root = include_docs.back()->Parse(ResLoader::Instance().Open(include_name));

					std::vector<std::string> include_names;
					this->RecursiveIncludeNode(*include_root, include_names);

					if (!include_names.empty())
					{
						for (auto iter = include_names.begin(); iter != include_names.end();)
						{
							bool found = false;
							for (auto iter_w = whole_include_names.begin(); iter_w != whole_include_names.end(); ++ iter_w)
							{
								if (*iter == *iter_w)
								{
									found = true;
									break;
								}
							}

							if (found)
							{
								iter = include_names.erase(iter);
							}
							else
							{
								include_docs.push_back(MakeUniquePtr<XMLDocument>());
								XMLNodePtr recursive_include_root = include_docs.back()->Parse(ResLoader::Instance().Open(*iter));
								this->InsertIncludeNodes(*doc, *root, node, *recursive_include_root);

								whole_include_names.push_back(*iter);
								++ iter;
							}
						}
					}

					bool found = false;
					for (auto iter_w = whole_include_names.begin(); iter_w != whole_include_names.end(); ++ iter_w)
					{
						if (include_name == *iter_w)
						{
							found = true;
							break;
						}
					}

					if (!found)
					{
						this->InsertIncludeNodes(*doc, *root, node, *include_root);
						whole_include_names.push_back(include_name);
					}

					XMLNodePtr node_next = node->NextSibling("include");
					root->RemoveNode(node);
					node = node_next;
				}

				{
					XMLNodePtr macro_node = root->FirstNode("macro");
					if (macro_node)
					{
						macros_ = MakeSharedPtr<std::remove_reference<decltype(*macros_)>::type>();
					}
					for (; macro_node; macro_node = macro_node->NextSibling("macro"))
					{
						macros_->emplace_back(std::make_pair(macro_node->Attrib("name")->ValueString(), macro_node->Attrib("value")->ValueString()), true);
					}
				}

				std::vector<XMLNodePtr> parameter_nodes;
				for (XMLNodePtr node = root->FirstNode(); node; node = node->NextSibling())
				{
					if ("parameter" == node->Name())
					{
						parameter_nodes.push_back(node);
					}
					else if ("cbuffer" == node->Name())
					{
						for (XMLNodePtr sub_node = node->FirstNode("parameter"); sub_node; sub_node = sub_node->NextSibling("parameter"))
						{
							parameter_nodes.push_back(sub_node);
						}
					}
				}

				for (uint32_t param_index = 0; param_index < parameter_nodes.size(); ++ param_index)
				{
					XMLNodePtr const & node = parameter_nodes[param_index];

					uint32_t type = type_define::instance().TypeCode(node->Attrib("type")->ValueString());
					if ((type != REDT_sampler)
						&& (type != REDT_texture1D) && (type != REDT_texture2D) && (type != REDT_texture3D)
						&& (type != REDT_textureCUBE)
						&& (type != REDT_texture1DArray) && (type != REDT_texture2DArray)
						&& (type != REDT_texture3DArray) && (type != REDT_textureCUBEArray)
						&& (type != REDT_buffer) && (type != REDT_structured_buffer)
						&& (type != REDT_byte_address_buffer) && (type != REDT_rw_buffer)
						&& (type != REDT_rw_structured_buffer) && (type != REDT_rw_texture1D)
						&& (type != REDT_rw_texture2D) && (type != REDT_rw_texture3D)
						&& (type != REDT_rw_texture1DArray) && (type != REDT_rw_texture2DArray)
						&& (type != REDT_rw_byte_address_buffer) && (type != REDT_append_structured_buffer)
						&& (type != REDT_consume_structured_buffer))
					{
						RenderEffectConstantBuffer* cbuff = nullptr;
						XMLNodePtr parent_node = node->Parent();
						std::string cbuff_name = parent_node->AttribString("name", "global_cb");
						size_t const cbuff_name_hash = RT_HASH(cbuff_name.c_str());

						bool found = false;
						for (size_t i = 0; i < effect.cbuffers_.size(); ++ i)
						{
							if (effect.cbuffers_[i]->NameHash() == cbuff_name_hash)
							{
								cbuff = effect.cbuffers_[i].get();
								found = true;
								break;
							}
						}
						if (!found)
						{
							effect.cbuffers_.push_back(MakeUniquePtr<RenderEffectConstantBuffer>());
							cbuff = effect.cbuffers_.back().get();
							cbuff->Load(cbuff_name);
						}
						BOOST_ASSERT(cbuff);

						cbuff->AddParameter(param_index);
					}

					effect.params_.push_back(MakeUniquePtr<RenderEffectParameter>());
					effect.params_.back()->Load(node);
				}

				for (XMLNodePtr shader_node = root->FirstNode("shader"); shader_node; shader_node = shader_node->NextSibling("shader"))
				{
					shader_frags_.push_back(RenderShaderFragment());
					shader_frags_.back().Load(shader_node);
				}

				this->GenHLSLShaderText(effect);

				uint32_t index = 0;
				for (XMLNodePtr node = root->FirstNode("technique"); node; node = node->NextSibling("technique"), ++ index)
				{
					techniques_.push_back(MakeUniquePtr<RenderTechnique>());
					techniques_.back()->Load(effect, node, index);
				}
			}

			std::ofstream ofs(kfx_name.c_str(), std::ios_base::binary | std::ios_base::out);
			this->StreamOut(ofs, effect);
#endif
		}
	}

	bool RenderEffectTemplate::StreamIn(ResIdentifierPtr const & source, RenderEffect& effect)
	{
		RenderEngine& re = Context::Instance().RenderFactoryInstance().RenderEngineInstance();

		bool ret = false;
		if (source)
		{
			uint32_t fourcc;
			source->read(&fourcc, sizeof(fourcc));
			fourcc = LE2Native(fourcc);
			
			uint32_t ver;
			source->read(&ver, sizeof(ver));
			ver = LE2Native(ver);
			
			if ((MakeFourCC<'K', 'F', 'X', ' '>::value == fourcc) && (KFX_VERSION == ver))
			{
				uint32_t shader_fourcc;
				source->read(&shader_fourcc, sizeof(shader_fourcc));
				shader_fourcc = LE2Native(shader_fourcc);

				uint32_t shader_ver;
				source->read(&shader_ver, sizeof(shader_ver));
				shader_ver = LE2Native(shader_ver);

				uint8_t shader_platform_name_len;
				source->read(&shader_platform_name_len, sizeof(shader_platform_name_len));
				std::string shader_platform_name(shader_platform_name_len, 0);
				source->read(&shader_platform_name[0], shader_platform_name_len);

				if ((re.NativeShaderFourCC() == shader_fourcc) && (re.NativeShaderVersion() == shader_ver)
					&& (re.NativeShaderPlatformName() == shader_platform_name))
				{
					uint64_t timestamp;
					source->read(&timestamp, sizeof(timestamp));
#if KLAYGE_IS_DEV_PLATFORM
					timestamp = LE2Native(timestamp);
					if (timestamp_ <= timestamp)
#endif
					{
						shader_descs_.resize(1);

						{
							uint16_t num_macros;
							source->read(&num_macros, sizeof(num_macros));
							num_macros = LE2Native(num_macros);

							if (num_macros > 0)
							{
								macros_ = MakeSharedPtr<std::remove_reference<decltype(*macros_)>::type>();
							}
							for (uint32_t i = 0; i < num_macros; ++ i)
							{
								std::string name = ReadShortString(source);
								std::string value = ReadShortString(source);
								macros_->emplace_back(std::make_pair(name, value), true);
							}
						}

						{
							uint16_t num_cbufs;
							source->read(&num_cbufs, sizeof(num_cbufs));
							num_cbufs = LE2Native(num_cbufs);
							effect.cbuffers_.resize(num_cbufs);
							for (uint32_t i = 0; i < num_cbufs; ++ i)
							{
								effect.cbuffers_[i] = MakeUniquePtr<RenderEffectConstantBuffer>();
								effect.cbuffers_[i]->StreamIn(source);
							}
						}

						{
							uint16_t num_params;
							source->read(&num_params, sizeof(num_params));
							num_params = LE2Native(num_params);
							effect.params_.resize(num_params);
							for (uint32_t i = 0; i < num_params; ++ i)
							{
								effect.params_[i] = MakeUniquePtr<RenderEffectParameter>();
								effect.params_[i]->StreamIn(source);
							}
						}

						{
							uint16_t num_shader_frags;
							source->read(&num_shader_frags, sizeof(num_shader_frags));
							num_shader_frags = LE2Native(num_shader_frags);
							if (num_shader_frags > 0)
							{
								shader_frags_.resize(num_shader_frags);
								for (uint32_t i = 0; i < num_shader_frags; ++ i)
								{
									shader_frags_[i].StreamIn(source);
								}
							}
						}

						{
							uint16_t num_shader_descs;
							source->read(&num_shader_descs, sizeof(num_shader_descs));
							num_shader_descs = LE2Native(num_shader_descs);
							shader_descs_.resize(num_shader_descs + 1);
							for (uint32_t i = 0; i < num_shader_descs; ++ i)
							{
								shader_descs_[i + 1].profile = ReadShortString(source);
								shader_descs_[i + 1].func_name = ReadShortString(source);
								source->read(&shader_descs_[i + 1].macros_hash, sizeof(shader_descs_[i + 1].macros_hash));

								source->read(&shader_descs_[i + 1].tech_pass_type, sizeof(shader_descs_[i + 1].tech_pass_type));
								shader_descs_[i + 1].tech_pass_type = LE2Native(shader_descs_[i + 1].tech_pass_type);

								uint8_t len;
								source->read(&len, sizeof(len));
								if (len > 0)
								{
									shader_descs_[i + 1].so_decl.resize(len);
									source->read(&shader_descs_[i + 1].so_decl[0], len * sizeof(shader_descs_[i + 1].so_decl[0]));
									for (uint32_t j = 0; j < len; ++ j)
									{
										shader_descs_[i + 1].so_decl[j].usage = LE2Native(shader_descs_[i + 1].so_decl[j].usage);
									}
								}
							}
						}

						ret = true;
						{
							uint16_t num_techs;
							source->read(&num_techs, sizeof(num_techs));
							num_techs = LE2Native(num_techs);
							techniques_.resize(num_techs);
							for (uint32_t i = 0; i < num_techs; ++ i)
							{
								techniques_[i] = MakeUniquePtr<RenderTechnique>();
								ret &= techniques_[i]->StreamIn(effect, source, i);
							}
						}
					}
				}
			}
		}

		return ret;
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectTemplate::StreamOut(std::ostream& os, RenderEffect const & effect) const
	{
		RenderEngine& re = Context::Instance().RenderFactoryInstance().RenderEngineInstance();

		uint32_t fourcc = Native2LE(MakeFourCC<'K', 'F', 'X', ' '>::value);
		os.write(reinterpret_cast<char const *>(&fourcc), sizeof(fourcc));

		uint32_t ver = Native2LE(KFX_VERSION);
		os.write(reinterpret_cast<char const *>(&ver), sizeof(ver));

		uint32_t shader_fourcc = Native2LE(re.NativeShaderFourCC());
		os.write(reinterpret_cast<char const *>(&shader_fourcc), sizeof(shader_fourcc));

		uint32_t shader_ver = Native2LE(re.NativeShaderVersion());
		os.write(reinterpret_cast<char const *>(&shader_ver), sizeof(shader_ver));

		uint8_t shader_platform_name_len = static_cast<uint8_t>(re.NativeShaderPlatformName().size());
		os.write(reinterpret_cast<char const *>(&shader_platform_name_len), sizeof(shader_platform_name_len));
		os.write(&re.NativeShaderPlatformName()[0], shader_platform_name_len);

		uint64_t timestamp = Native2LE(timestamp_);
		os.write(reinterpret_cast<char const *>(&timestamp), sizeof(timestamp));

		{
			uint16_t num_macros = 0;
			if (macros_)
			{
				for (uint32_t i = 0; i < macros_->size(); ++ i)
				{
					if ((*macros_)[i].second)
					{
						++ num_macros;
					}
				}
			}

			num_macros = Native2LE(num_macros);
			os.write(reinterpret_cast<char const *>(&num_macros), sizeof(num_macros));

			if (macros_)
			{
				for (uint32_t i = 0; i < macros_->size(); ++ i)
				{
					if ((*macros_)[i].second)
					{
						WriteShortString(os, (*macros_)[i].first.first);
						WriteShortString(os, (*macros_)[i].first.second);
					}
				}
			}
		}

		{
			uint16_t num_cbufs = Native2LE(static_cast<uint16_t>(effect.cbuffers_.size()));
			os.write(reinterpret_cast<char const *>(&num_cbufs), sizeof(num_cbufs));
			for (uint32_t i = 0; i < effect.cbuffers_.size(); ++ i)
			{
				effect.cbuffers_[i]->StreamOut(os);
			}
		}

		{
			uint16_t num_params = Native2LE(static_cast<uint16_t>(effect.params_.size()));
			os.write(reinterpret_cast<char const *>(&num_params), sizeof(num_params));
			for (uint32_t i = 0; i < effect.params_.size(); ++ i)
			{
				effect.params_[i]->StreamOut(os);
			}
		}

		{
			uint16_t num_shader_frags = Native2LE(static_cast<uint16_t>(shader_frags_.size()));
			os.write(reinterpret_cast<char const *>(&num_shader_frags), sizeof(num_shader_frags));
			for (uint32_t i = 0; i < shader_frags_.size(); ++ i)
			{
				shader_frags_[i].StreamOut(os);
			}
		}

		{
			uint16_t num_shader_descs = Native2LE(static_cast<uint16_t>(shader_descs_.size() - 1));
			os.write(reinterpret_cast<char const *>(&num_shader_descs), sizeof(num_shader_descs));
			for (uint32_t i = 0; i < shader_descs_.size() - 1; ++ i)
			{
				WriteShortString(os, shader_descs_[i + 1].profile);
				WriteShortString(os, shader_descs_[i + 1].func_name);

				uint64_t tmp64 = Native2LE(shader_descs_[i + 1].macros_hash);
				os.write(reinterpret_cast<char const *>(&tmp64), sizeof(tmp64));

				uint32_t tmp32 = Native2LE(shader_descs_[i + 1].tech_pass_type);
				os.write(reinterpret_cast<char const *>(&tmp32), sizeof(tmp32));

				uint8_t len = static_cast<uint8_t>(shader_descs_[i + 1].so_decl.size());
				os.write(reinterpret_cast<char const *>(&len), sizeof(len));
				for (uint32_t j = 0; j < len; ++ j)
				{
					ShaderDesc::StreamOutputDecl so_decl = shader_descs_[i + 1].so_decl[j];
					so_decl.usage = Native2LE(so_decl.usage);
					os.write(reinterpret_cast<char const *>(&so_decl), sizeof(so_decl));
				}
			}
		}

		{
			uint16_t num_techs = Native2LE(static_cast<uint16_t>(techniques_.size()));
			os.write(reinterpret_cast<char const *>(&num_techs), sizeof(num_techs));
			for (uint32_t i = 0; i < techniques_.size(); ++ i)
			{
				techniques_[i]->StreamOut(effect, os, i);
			}
		}
	}
#endif

	RenderTechnique* RenderEffectTemplate::TechniqueByName(std::string_view name) const
	{
		size_t const name_hash = HashRange(name.begin(), name.end());
		for (auto const & tech : techniques_)
		{
			if (name_hash == tech->NameHash())
			{
				return tech.get();
			}
		}
		return nullptr;
	}

	uint32_t RenderEffectTemplate::AddShaderDesc(ShaderDesc const & sd)
	{
		for (uint32_t i = 0; i < shader_descs_.size(); ++ i)
		{
			if (shader_descs_[i] == sd)
			{
				return i;
			}
		}

		uint32_t id = static_cast<uint32_t>(shader_descs_.size());
		shader_descs_.push_back(sd);
		return id;
	}

	ShaderDesc& RenderEffectTemplate::GetShaderDesc(uint32_t id)
	{
		BOOST_ASSERT(id < shader_descs_.size());
		return shader_descs_[id];
	}

	ShaderDesc const & RenderEffectTemplate::GetShaderDesc(uint32_t id) const
	{
		BOOST_ASSERT(id < shader_descs_.size());
		return shader_descs_[id];
	}

	std::string const & RenderEffectTemplate::TypeName(uint32_t code) const
	{
		return type_define::instance().TypeName(code);
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectTemplate::GenHLSLShaderText(RenderEffect const & effect)
	{
		std::string& str = hlsl_shader_;

		str += "#define SHADER_MODEL(major, minor) ((major) * 4 + (minor))\n\n";

		for (uint32_t i = 0; i < this->NumMacros(); ++ i)
		{
			std::pair<std::string, std::string> const & name_value = this->MacroByIndex(i);
			str += "#define " + name_value.first + " " + name_value.second + "\n";
		}
		str += '\n';

		for (uint32_t i = 0; i < effect.NumCBuffers(); ++ i)
		{
			RenderEffectConstantBuffer const & cbuff = *effect.CBufferByIndex(i);
			str += "cbuffer " + cbuff.Name() + "\n";
			str += "{\n";

			for (uint32_t j = 0; j < cbuff.NumParameters(); ++ j)
			{
				RenderEffectParameter const & param = *effect.ParameterByIndex(cbuff.ParameterIndex(j));
				switch (param.Type())
				{
				case REDT_texture1D:
				case REDT_texture2D:
				case REDT_texture3D:
				case REDT_textureCUBE:
				case REDT_texture1DArray:
				case REDT_texture2DArray:
				case REDT_texture3DArray:
				case REDT_textureCUBEArray:
				case REDT_sampler:
				case REDT_buffer:
				case REDT_structured_buffer:
				case REDT_byte_address_buffer:
				case REDT_rw_buffer:
				case REDT_rw_structured_buffer:
				case REDT_rw_texture1D:
				case REDT_rw_texture2D:
				case REDT_rw_texture3D:
				case REDT_rw_texture1DArray:
				case REDT_rw_texture2DArray:
				case REDT_rw_byte_address_buffer:
				case REDT_append_structured_buffer:
				case REDT_consume_structured_buffer:
					break;

				default:
					str += this->TypeName(param.Type()) + " " + param.Name();
					if (param.ArraySize())
					{
						str += "[" + *param.ArraySize() + "]";
					}
					str += ";\n";
					break;
				}
			}

			str += "};\n";
		}

		for (uint32_t i = 0; i < effect.NumParameters(); ++ i)
		{
			RenderEffectParameter& param = *effect.ParameterByIndex(i);

			std::string elem_type;
			switch (param.Type())
			{
			case REDT_texture1D:
			case REDT_texture2D:
			case REDT_texture3D:
			case REDT_textureCUBE:
			case REDT_texture1DArray:
			case REDT_texture2DArray:
			case REDT_textureCUBEArray:
			case REDT_buffer:
			case REDT_structured_buffer:
			case REDT_rw_buffer:
			case REDT_rw_structured_buffer:
			case REDT_rw_texture1D:
			case REDT_rw_texture2D:
			case REDT_rw_texture3D:
			case REDT_rw_texture1DArray:
			case REDT_rw_texture2DArray:
			case REDT_append_structured_buffer:
			case REDT_consume_structured_buffer:
				param.Var().Value(elem_type);
				break;

			default:
				break;
			}

			std::string const & param_name = param.Name();
			switch (param.Type())
			{
			case REDT_texture1D:
				str += "Texture1D<" + elem_type + "> " + param_name + ";\n";
				break;

			case REDT_texture2D:
				str += "Texture2D<" + elem_type + "> " + param_name + ";\n";
				break;

			case REDT_texture3D:
				str += "#if KLAYGE_MAX_TEX_DEPTH <= 1\n";
				str += "Texture2D<" + elem_type + "> " + param_name + ";\n";
				str += "#else\n";
				str += "Texture3D<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_textureCUBE:
				str += "TextureCube<" + elem_type + "> " + param_name + ";\n";
				break;

			case REDT_texture1DArray:
				str += "#if KLAYGE_MAX_TEX_ARRAY_LEN > 1\n";
				str += "Texture1DArray<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_texture2DArray:
				str += "#if KLAYGE_MAX_TEX_ARRAY_LEN > 1\n";
				str += "Texture2DArray<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_textureCUBEArray:
				str += "#if (KLAYGE_MAX_TEX_ARRAY_LEN > 1) && (KLAYGE_SHADER_MODEL >= SHADER_MODEL(4, 1))\n";
				str += "TextureCubeArray<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_buffer:
				str += "Buffer<" + elem_type + "> " + param_name + ";\n";
				break;

			case REDT_sampler:
				str += "sampler " + param_name + ";\n";
				break;

			case REDT_structured_buffer:
				str += "StructuredBuffer<" + elem_type + "> " + param_name + ";\n";
				break;

			case REDT_byte_address_buffer:
				str += "ByteAddressBuffer " + param_name + ";\n";
				break;

			case REDT_rw_buffer:
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL(5, 0)\n";
				str += "RWBuffer<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_rw_structured_buffer:
				str += "RWStructuredBuffer<" + elem_type + "> " + param_name + ";\n";
				break;

			case REDT_rw_texture1D:
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL(5, 0)\n";
				str += "RWTexture1D<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_rw_texture2D:
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL(5, 0)\n";
				str += "RWTexture2D<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_rw_texture3D:
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL(5, 0)\n";
				str += "RWTexture3D<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_rw_texture1DArray:
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL(5, 0)\n";
				str += "RWTexture1DArray<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_rw_texture2DArray:
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL(5, 0)\n";
				str += "RWTexture2DArray<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_rw_byte_address_buffer:
				str += "RWByteAddressBuffer " + param_name + ";\n";
				break;

			case REDT_append_structured_buffer:
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL(5, 0)\n";
				str += "AppendStructuredBuffer<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			case REDT_consume_structured_buffer:
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL(5, 0)\n";
				str += "ConsumeStructuredBuffer<" + elem_type + "> " + param_name + ";\n";
				str += "#endif\n";
				break;

			default:
				break;
			}
		}

		for (uint32_t i = 0; i < this->NumShaderFragments(); ++ i)
		{
			RenderShaderFragment const & effect_shader_frag = this->ShaderFragmentByIndex(i);
			ShaderObject::ShaderType const shader_type = effect_shader_frag.Type();
			switch (shader_type)
			{
			case ShaderObject::ST_VertexShader:
				str += "#if KLAYGE_VERTEX_SHADER\n";
				break;

			case ShaderObject::ST_PixelShader:
				str += "#if KLAYGE_PIXEL_SHADER\n";
				break;

			case ShaderObject::ST_GeometryShader:
				str += "#if KLAYGE_GEOMETRY_SHADER\n";
				break;

			case ShaderObject::ST_ComputeShader:
				str += "#if KLAYGE_COMPUTE_SHADER\n";
				break;

			case ShaderObject::ST_HullShader:
				str += "#if KLAYGE_HULL_SHADER\n";
				break;

			case ShaderObject::ST_DomainShader:
				str += "#if KLAYGE_DOMAIN_SHADER\n";
				break;

			case ShaderObject::ST_NumShaderTypes:
				break;

			default:
				KFL_UNREACHABLE("Invalid shader type");
			}
			ShaderModel const & ver = effect_shader_frag.Version();
			if ((ver.major_ver != 0) || (ver.minor_ver != 0))
			{
				str += "#if KLAYGE_SHADER_MODEL >= SHADER_MODEL("
					+ boost::lexical_cast<std::string>(static_cast<int>(ver.major_ver)) + ", "
					+ boost::lexical_cast<std::string>(static_cast<int>(ver.minor_ver)) + ")\n";
			}

			str += effect_shader_frag.str() + "\n";

			if ((ver.major_ver != 0) || (ver.minor_ver != 0))
			{
				str += "#endif\n";
			}
			if (shader_type != ShaderObject::ST_NumShaderTypes)
			{
				str += "#endif\n";
			}
		}
	}
#endif


#if KLAYGE_IS_DEV_PLATFORM
	void RenderTechnique::Load(RenderEffect& effect, XMLNodePtr const & node, uint32_t tech_index)
	{
		name_ = node->Attrib("name")->ValueString();
		name_hash_ = HashRange(name_.begin(), name_.end());

		RenderTechnique* parent_tech = nullptr;
		XMLAttributePtr inherit_attr = node->Attrib("inherit");
		if (inherit_attr)
		{
			std::string inherit = inherit_attr->ValueString();
			BOOST_ASSERT(inherit != name_);

			parent_tech = effect.TechniqueByName(inherit);
			BOOST_ASSERT(parent_tech);
		}

		{
			XMLNodePtr anno_node = node->FirstNode("annotation");
			if (anno_node)
			{
				annotations_ = MakeSharedPtr<std::remove_reference<decltype(*annotations_)>::type>();
				if (parent_tech && parent_tech->annotations_)
				{
					*annotations_ = *parent_tech->annotations_;
				}
				for (; anno_node; anno_node = anno_node->NextSibling("annotation"))
				{
					RenderEffectAnnotationPtr annotation = MakeSharedPtr<RenderEffectAnnotation>();
					annotations_->push_back(annotation);

					annotation->Load(anno_node);
				}
			}
			else if (parent_tech)
			{
				annotations_ = parent_tech->annotations_;
			}
		}

		{
			XMLNodePtr macro_node = node->FirstNode("macro");
			if (macro_node)
			{
				macros_ = MakeSharedPtr<std::remove_reference<decltype(*macros_)>::type>();
				if (parent_tech && parent_tech->macros_)
				{
					*macros_ = *parent_tech->macros_;
				}
				for (; macro_node; macro_node = macro_node->NextSibling("macro"))
				{
					std::string name = macro_node->Attrib("name")->ValueString();
					std::string value = macro_node->Attrib("value")->ValueString();
					bool found = false;
					for (size_t i = 0; i < macros_->size(); ++ i)
					{
						if ((*macros_)[i].first == name)
						{
							(*macros_)[i].second = value;
							found = true;
							break;
						}
					}
					if (!found)
					{
						macros_->emplace_back(name, value);
					}
				}
			}
			else if (parent_tech)
			{
				macros_ = parent_tech->macros_;
			}
		}

		if (!node->FirstNode("pass") && parent_tech)
		{
			is_validate_ = parent_tech->is_validate_;
			has_discard_ = parent_tech->has_discard_;
			has_tessellation_ = parent_tech->has_tessellation_;
			transparent_ = parent_tech->transparent_;
			weight_ = parent_tech->weight_;

			if (macros_ == parent_tech->macros_)
			{
				passes_ = parent_tech->passes_;
			}
			else
			{
				for (uint32_t index = 0; index < parent_tech->passes_.size(); ++ index)
				{
					RenderPassPtr pass = MakeSharedPtr<RenderPass>();
					passes_.push_back(pass);

					auto inherit_pass = parent_tech->passes_[index].get();

					pass->Load(effect, tech_index, index, inherit_pass);
					is_validate_ &= pass->Validate();
				}
			}
		}
		else
		{
			is_validate_ = true;

			has_discard_ = false;
			has_tessellation_ = false;
			transparent_ = false;
			if (parent_tech)
			{
				weight_ = parent_tech->Weight();
			}
			else
			{
				weight_ = 1;
			}
		
			uint32_t index = 0;
			for (XMLNodePtr pass_node = node->FirstNode("pass"); pass_node; pass_node = pass_node->NextSibling("pass"), ++ index)
			{
				RenderPassPtr pass = MakeSharedPtr<RenderPass>();
				passes_.push_back(pass);

				RenderPass* inherit_pass = nullptr;
				if (parent_tech && (index < parent_tech->passes_.size()))
				{
					inherit_pass = parent_tech->passes_[index].get();
				}

				pass->Load(effect, pass_node, tech_index, index, inherit_pass);

				is_validate_ &= pass->Validate();

				for (XMLNodePtr state_node = pass_node->FirstNode("state"); state_node; state_node = state_node->NextSibling("state"))
				{
					++ weight_;

					std::string state_name = state_node->Attrib("name")->ValueString();
					if ("blend_enable" == state_name)
					{
						std::string value_str = state_node->Attrib("value")->ValueString();
						if (BoolFromStr(value_str))
						{
							transparent_ = true;
						}
					}
				}

				has_discard_ |= pass->GetShaderObject(effect)->HasDiscard();
				has_tessellation_ |= pass->GetShaderObject(effect)->HasTessellation();
			}
			if (transparent_)
			{
				weight_ += 10000;
			}
		}
	}
#endif

	bool RenderTechnique::StreamIn(RenderEffect& effect, ResIdentifierPtr const & res, uint32_t tech_index)
	{
		name_ = ReadShortString(res);
		name_hash_ = HashRange(name_.begin(), name_.end());

		uint8_t num_anno;
		res->read(&num_anno, sizeof(num_anno));
		if (num_anno > 0)
		{
			annotations_ = MakeSharedPtr<std::remove_reference<decltype(*annotations_)>::type>();
			annotations_->resize(num_anno);
			for (uint32_t i = 0; i < num_anno; ++ i)
			{
				RenderEffectAnnotationPtr annotation = MakeSharedPtr<RenderEffectAnnotation>();
				(*annotations_)[i] = annotation;
				
				annotation->StreamIn(res);
			}
		}

		uint8_t num_macro;
		res->read(&num_macro, sizeof(num_macro));
		if (num_macro > 0)
		{
			macros_ = MakeSharedPtr<std::remove_reference<decltype(*macros_)>::type>();
			macros_->resize(num_macro);
			for (uint32_t i = 0; i < num_macro; ++ i)
			{
				std::string name = ReadShortString(res);
				std::string value = ReadShortString(res);
				(*macros_)[i] = std::make_pair(name, value);
			}
		}

		is_validate_ = true;

		has_discard_ = false;
		has_tessellation_ = false;
		
		res->read(&transparent_, sizeof(transparent_));
		res->read(&weight_, sizeof(weight_));
		weight_ = LE2Native(weight_);

		bool ret = true;
		uint8_t num_passes;
		res->read(&num_passes, sizeof(num_passes));
		passes_.resize(num_passes);
		for (uint32_t pass_index = 0; pass_index < num_passes; ++ pass_index)
		{
			RenderPassPtr pass = MakeSharedPtr<RenderPass>();
			passes_[pass_index] = pass;

			ret &= pass->StreamIn(effect, res, tech_index, pass_index);

			is_validate_ &= pass->Validate();

			has_discard_ |= pass->GetShaderObject(effect)->HasDiscard();
			has_tessellation_ |= pass->GetShaderObject(effect)->HasTessellation();
		}

		return ret;
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderTechnique::StreamOut(RenderEffect const & effect, std::ostream& os, uint32_t tech_index) const
	{
		WriteShortString(os, name_);

		uint8_t num_anno;
		if (annotations_)
		{
			num_anno = static_cast<uint8_t>(annotations_->size());
		}
		else
		{
			num_anno = 0;
		}
		os.write(reinterpret_cast<char const *>(&num_anno), sizeof(num_anno));
		for (uint32_t i = 0; i < num_anno; ++ i)
		{
			RenderEffectAnnotationPtr annotation = MakeSharedPtr<RenderEffectAnnotation>();
			(*annotations_)[i] = annotation;
				
			annotation->StreamOut(os);
		}

		uint8_t num_macro;
		if (macros_)
		{
			num_macro = static_cast<uint8_t>(macros_->size());
		}
		else
		{
			num_macro = 0;
		}
		os.write(reinterpret_cast<char const *>(&num_macro), sizeof(num_macro));
		for (uint32_t i = 0; i < num_macro; ++ i)
		{
			WriteShortString(os, (*macros_)[i].first);
			WriteShortString(os, (*macros_)[i].second);
		}

		os.write(reinterpret_cast<char const *>(&transparent_), sizeof(transparent_));
		float w = Native2LE(weight_);
		os.write(reinterpret_cast<char const *>(&w), sizeof(w));

		uint8_t num_passes = static_cast<uint8_t>(passes_.size());
		os.write(reinterpret_cast<char const *>(&num_passes), sizeof(num_passes));
		for (uint32_t pass_index = 0; pass_index < num_passes; ++ pass_index)
		{
			passes_[pass_index]->StreamOut(effect, os, tech_index, pass_index);
		}
	}
#endif


#if KLAYGE_IS_DEV_PLATFORM
	void RenderPass::Load(RenderEffect& effect, XMLNodePtr const & node,
		uint32_t tech_index, uint32_t pass_index, RenderPass const * inherit_pass)
	{
		RenderFactory& rf = Context::Instance().RenderFactoryInstance();

		name_ = node->Attrib("name")->ValueString();
		name_hash_ = HashRange(name_.begin(), name_.end());

		{
			XMLNodePtr anno_node = node->FirstNode("annotation");
			if (anno_node)
			{
				annotations_ = MakeSharedPtr<std::remove_reference<decltype(*annotations_)>::type>();
				if (inherit_pass && inherit_pass->annotations_)
				{
					*annotations_ = *inherit_pass->annotations_;
				}
				for (; anno_node; anno_node = anno_node->NextSibling("annotation"))
				{
					RenderEffectAnnotationPtr annotation = MakeSharedPtr<RenderEffectAnnotation>();
					annotations_->push_back(annotation);

					annotation->Load(anno_node);
				}
			}
			else if (inherit_pass)
			{
				annotations_ = inherit_pass->annotations_;
			}
		}

		{
			XMLNodePtr macro_node = node->FirstNode("macro");
			if (macro_node)
			{
				macros_ = MakeSharedPtr<std::remove_reference<decltype(*macros_)>::type>();
				if (inherit_pass && inherit_pass->macros_)
				{
					*macros_ = *inherit_pass->macros_;
				}
				for (; macro_node; macro_node = macro_node->NextSibling("macro"))
				{
					std::string name = macro_node->Attrib("name")->ValueString();
					std::string value = macro_node->Attrib("value")->ValueString();
					bool found = false;
					for (size_t i = 0; i < macros_->size(); ++ i)
					{
						if ((*macros_)[i].first == name)
						{
							(*macros_)[i].second = value;
							found = true;
							break;
						}
					}
					if (!found)
					{
						macros_->emplace_back(name, value);
					}
				}
			}
			else if (inherit_pass)
			{
				macros_ = inherit_pass->macros_;
			}
		}

		uint64_t macros_hash;
		{
			RenderTechnique* tech = effect.TechniqueByIndex(tech_index);

			size_t hash_val = 0;
			for (uint32_t i = 0; i < tech->NumMacros(); ++ i)
			{
				std::pair<std::string, std::string> const & name_value = tech->MacroByIndex(i);
				HashRange(hash_val, name_value.first.begin(), name_value.first.end());
				HashRange(hash_val, name_value.second.begin(), name_value.second.end());
			}
			for (uint32_t i = 0; i < this->NumMacros(); ++ i)
			{
				std::pair<std::string, std::string> const & name_value = this->MacroByIndex(i);
				HashRange(hash_val, name_value.first.begin(), name_value.first.end());
				HashRange(hash_val, name_value.second.begin(), name_value.second.end());
			}
			macros_hash = static_cast<uint64_t>(hash_val);
		}

		RasterizerStateDesc rs_desc;
		DepthStencilStateDesc dss_desc;
		BlendStateDesc bs_desc;
		shader_obj_index_ = effect.AddShaderObject();

		shader_desc_ids_.fill(0);

		if (inherit_pass)
		{
			rs_desc = inherit_pass->render_state_obj_->GetRasterizerStateDesc();
			dss_desc = inherit_pass->render_state_obj_->GetDepthStencilStateDesc();
			bs_desc = inherit_pass->render_state_obj_->GetBlendStateDesc();
			shader_desc_ids_ = inherit_pass->shader_desc_ids_;
		}

		for (XMLNodePtr state_node = node->FirstNode("state"); state_node; state_node = state_node->NextSibling("state"))
		{
			std::string state_name = state_node->Attrib("name")->ValueString();
			size_t const state_name_hash = RT_HASH(state_name.c_str());

			if (CT_HASH("polygon_mode") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				rs_desc.polygon_mode = polygon_mode_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("shade_mode") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				rs_desc.shade_mode = shade_mode_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("cull_mode") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				rs_desc.cull_mode = cull_mode_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("front_face_ccw") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				rs_desc.front_face_ccw = BoolFromStr(value_str);
			}
			else if (CT_HASH("polygon_offset_factor") == state_name_hash)
			{
				rs_desc.polygon_offset_factor = state_node->Attrib("value")->ValueFloat();
			}
			else if (CT_HASH("polygon_offset_units") == state_name_hash)
			{
				rs_desc.polygon_offset_units = state_node->Attrib("value")->ValueFloat();
			}
			else if (CT_HASH("depth_clip_enable") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				rs_desc.depth_clip_enable = BoolFromStr(value_str);
			}
			else if (CT_HASH("scissor_enable") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				rs_desc.scissor_enable = BoolFromStr(value_str);
			}
			else if (CT_HASH("multisample_enable") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				rs_desc.multisample_enable = BoolFromStr(value_str);
			}
			else if (CT_HASH("alpha_to_coverage_enable") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.alpha_to_coverage_enable = BoolFromStr(value_str);
			}
			else if (CT_HASH("independent_blend_enable") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.independent_blend_enable = BoolFromStr(value_str);
			}
			else if (CT_HASH("blend_enable") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.blend_enable[index] = BoolFromStr(value_str);
			}
			else if (CT_HASH("logic_op_enable") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.logic_op_enable[index] = BoolFromStr(value_str);
			}
			else if (CT_HASH("blend_op") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.blend_op[index] = blend_operation_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("src_blend") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.src_blend[index] = alpha_blend_factor_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("dest_blend") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.dest_blend[index] = alpha_blend_factor_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("blend_op_alpha") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.blend_op_alpha[index] = blend_operation_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("src_blend_alpha") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.src_blend_alpha[index] = alpha_blend_factor_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("dest_blend_alpha") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.dest_blend_alpha[index] = alpha_blend_factor_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("logic_op") == state_name_hash)
			{
				int index = get_index(state_node);
				std::string value_str = state_node->Attrib("value")->ValueString();
				bs_desc.logic_op[index] = logic_operation_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("color_write_mask") == state_name_hash)
			{
				int index = get_index(state_node);
				bs_desc.color_write_mask[index] = static_cast<uint8_t>(state_node->Attrib("value")->ValueUInt());
			}
			else if (CT_HASH("blend_factor") == state_name_hash)
			{
				XMLAttributePtr attr = state_node->Attrib("r");
				if (attr)
				{
					bs_desc.blend_factor.r() = attr->ValueFloat();
				}
				attr = state_node->Attrib("g");
				if (attr)
				{
					bs_desc.blend_factor.g() = attr->ValueFloat();
				}
				attr = state_node->Attrib("b");
				if (attr)
				{
					bs_desc.blend_factor.b() = attr->ValueFloat();
				}
				attr = state_node->Attrib("a");
				if (attr)
				{
					bs_desc.blend_factor.a() = attr->ValueFloat();
				}
			}
			else if (CT_HASH("sample_mask") == state_name_hash)
			{
				bs_desc.sample_mask = state_node->Attrib("value")->ValueUInt();
			}
			else if (CT_HASH("depth_enable") == state_name_hash)
			{
				dss_desc.depth_enable = BoolFromStr(state_node->Attrib("value")->ValueString());
			}
			else if (CT_HASH("depth_write_mask") == state_name_hash)
			{
				dss_desc.depth_write_mask = BoolFromStr(state_node->Attrib("value")->ValueString());
			}
			else if (CT_HASH("depth_func") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.depth_func = compare_function_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("front_stencil_enable") == state_name_hash)
			{
				dss_desc.front_stencil_enable = BoolFromStr(state_node->Attrib("value")->ValueString());
			}
			else if (CT_HASH("front_stencil_func") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.front_stencil_func = compare_function_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("front_stencil_ref") == state_name_hash)
			{
				dss_desc.front_stencil_ref = static_cast<uint16_t>(state_node->Attrib("value")->ValueUInt());
			}
			else if (CT_HASH("front_stencil_read_mask") == state_name_hash)
			{
				dss_desc.front_stencil_read_mask = static_cast<uint16_t>(state_node->Attrib("value")->ValueUInt());
			}
			else if (CT_HASH("front_stencil_write_mask") == state_name_hash)
			{
				dss_desc.front_stencil_write_mask = static_cast<uint16_t>(state_node->Attrib("value")->ValueUInt());
			}
			else if (CT_HASH("front_stencil_fail") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.front_stencil_fail = stencil_operation_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("front_stencil_depth_fail") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.front_stencil_depth_fail = stencil_operation_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("front_stencil_pass") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.front_stencil_pass = stencil_operation_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("back_stencil_enable") == state_name_hash)
			{
				dss_desc.back_stencil_enable = BoolFromStr(state_node->Attrib("value")->ValueString());
			}
			else if (CT_HASH("back_stencil_func") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.back_stencil_func = compare_function_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("back_stencil_ref") == state_name_hash)
			{
				dss_desc.back_stencil_ref = static_cast<uint16_t>(state_node->Attrib("value")->ValueUInt());
			}
			else if (CT_HASH("back_stencil_read_mask") == state_name_hash)
			{
				dss_desc.back_stencil_read_mask = static_cast<uint16_t>(state_node->Attrib("value")->ValueUInt());
			}
			else if (CT_HASH("back_stencil_write_mask") == state_name_hash)
			{
				dss_desc.back_stencil_write_mask = static_cast<uint16_t>(state_node->Attrib("value")->ValueUInt());
			}
			else if (CT_HASH("back_stencil_fail") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.back_stencil_fail = stencil_operation_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("back_stencil_depth_fail") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.back_stencil_depth_fail = stencil_operation_define::instance().FromStr(value_str);
			}
			else if (CT_HASH("back_stencil_pass") == state_name_hash)
			{
				std::string value_str = state_node->Attrib("value")->ValueString();
				dss_desc.back_stencil_pass = stencil_operation_define::instance().FromStr(value_str);
			}
			else if ((CT_HASH("vertex_shader") == state_name_hash) || (CT_HASH("pixel_shader") == state_name_hash)
				|| (CT_HASH("geometry_shader") == state_name_hash) || (CT_HASH("compute_shader") == state_name_hash)
				|| (CT_HASH("hull_shader") == state_name_hash) || (CT_HASH("domain_shader") == state_name_hash))
			{
				ShaderObject::ShaderType type;
				if (CT_HASH("vertex_shader") == state_name_hash)
				{
					type = ShaderObject::ST_VertexShader;
				}
				else if (CT_HASH("pixel_shader") == state_name_hash)
				{
					type = ShaderObject::ST_PixelShader;
				}
				else if (CT_HASH("geometry_shader") == state_name_hash)
				{
					type = ShaderObject::ST_GeometryShader;
				}
				else if (CT_HASH("compute_shader") == state_name_hash)
				{
					type = ShaderObject::ST_ComputeShader;
				}
				else if (CT_HASH("hull_shader") == state_name_hash)
				{
					type = ShaderObject::ST_HullShader;
				}
				else
				{
					BOOST_ASSERT("domain_shader" == state_name);
					type = ShaderObject::ST_DomainShader;
				}

				ShaderDesc sd;
				sd.profile = get_profile(state_node);
				sd.func_name = get_func_name(state_node);
				sd.macros_hash = macros_hash;

				if ((ShaderObject::ST_VertexShader == type) || (ShaderObject::ST_GeometryShader == type))
				{
					XMLNodePtr so_node = state_node->FirstNode("stream_output");
					if (so_node)
					{
						for (XMLNodePtr entry_node = so_node->FirstNode("entry"); entry_node; entry_node = entry_node->NextSibling("entry"))
						{
							ShaderDesc::StreamOutputDecl decl;

							std::string usage_str = entry_node->Attrib("usage")->ValueString();
							size_t const usage_str_hash = RT_HASH(usage_str.c_str());
							XMLAttributePtr attr = entry_node->Attrib("usage_index");
							if (attr)
							{
								decl.usage_index = static_cast<uint8_t>(attr->ValueInt());
							}
							else
							{
								decl.usage_index = 0;
							}

							if ((CT_HASH("POSITION") == usage_str_hash) || (CT_HASH("SV_Position") == usage_str_hash))
							{
								decl.usage = VEU_Position;
							}
							else if (CT_HASH("NORMAL") == usage_str_hash)
							{
								decl.usage = VEU_Normal;
							}
							else if (CT_HASH("COLOR") == usage_str_hash)
							{
								if (0 == decl.usage_index)
								{
									decl.usage = VEU_Diffuse;
								}
								else
								{
									decl.usage = VEU_Specular;
								}
							}
							else if (CT_HASH("BLENDWEIGHT") == usage_str_hash)
							{
								decl.usage = VEU_BlendWeight;
							}
							else if (CT_HASH("BLENDINDICES") == usage_str_hash)
							{
								decl.usage = VEU_BlendIndex;
							}
							else if (CT_HASH("TEXCOORD") == usage_str_hash)
							{
								decl.usage = VEU_TextureCoord;
							}
							else if (CT_HASH("TANGENT") == usage_str_hash)
							{
								decl.usage = VEU_Tangent;
							}
							else if (CT_HASH("BINORMAL") == usage_str_hash)
							{
								decl.usage = VEU_Binormal;
							}

							attr = entry_node->Attrib("component");
							std::string component_str;
							if (attr)
							{
								component_str = entry_node->Attrib("component")->ValueString();
							}
							else
							{
								component_str = "xyzw";
							}
							decl.start_component = static_cast<uint8_t>(component_str[0] - 'x');
							decl.component_count = static_cast<uint8_t>(std::min(static_cast<size_t>(4), component_str.size()));

							attr = entry_node->Attrib("slot");
							if (attr)
							{
								decl.slot = static_cast<uint8_t>(entry_node->Attrib("slot")->ValueInt());
							}
							else
							{
								decl.slot = 0;
							}

							sd.so_decl.push_back(decl);
						}
					}
				}

				shader_desc_ids_[type] = effect.AddShaderDesc(sd);
			}
			else
			{
				KFL_UNREACHABLE("Invalid state name");
			}
		}

		render_state_obj_ = rf.MakeRenderStateObject(rs_desc, dss_desc, bs_desc);

		auto const & shader_obj = this->GetShaderObject(effect);

		for (int type = 0; type < ShaderObject::ST_NumShaderTypes; ++ type)
		{
			ShaderDesc& sd = effect.GetShaderDesc(shader_desc_ids_[type]);
			if (!sd.func_name.empty())
			{
				if (sd.tech_pass_type != 0xFFFFFFFF)
				{
					auto const & tech = *effect.TechniqueByIndex(sd.tech_pass_type >> 16);
					auto const & pass = tech.Pass((sd.tech_pass_type >> 8) & 0xFF);
					shader_obj->AttachShader(static_cast<ShaderObject::ShaderType>(type),
						effect, tech, pass, pass.GetShaderObject(effect));
				}
				else
				{
					auto const & tech = *effect.TechniqueByIndex(tech_index);
					shader_obj->AttachShader(static_cast<ShaderObject::ShaderType>(type),
						effect, tech, *this, shader_desc_ids_);
					sd.tech_pass_type = (tech_index << 16) + (pass_index << 8) + type;
				}
			}
		}

		shader_obj->LinkShaders(effect);

		is_validate_ = shader_obj->Validate();
	}

	void RenderPass::Load(RenderEffect& effect,
		uint32_t tech_index, uint32_t pass_index, RenderPass const * inherit_pass)
	{
		BOOST_ASSERT(inherit_pass);

		name_ = inherit_pass->name_;
		annotations_ = inherit_pass->annotations_;
		macros_ = inherit_pass->macros_;

		uint64_t macros_hash;
		{
			auto const & tech = *effect.TechniqueByIndex(tech_index);

			size_t hash_val = 0;
			for (uint32_t i = 0; i < tech.NumMacros(); ++ i)
			{
				std::pair<std::string, std::string> const & name_value = tech.MacroByIndex(i);
				HashRange(hash_val, name_value.first.begin(), name_value.first.end());
				HashRange(hash_val, name_value.second.begin(), name_value.second.end());
			}
			for (uint32_t i = 0; i < this->NumMacros(); ++ i)
			{
				std::pair<std::string, std::string> const & name_value = this->MacroByIndex(i);
				HashRange(hash_val, name_value.first.begin(), name_value.first.end());
				HashRange(hash_val, name_value.second.begin(), name_value.second.end());
			}
			macros_hash = static_cast<uint64_t>(hash_val);
		}

		shader_obj_index_ = effect.AddShaderObject();
		auto const & shader_obj = this->GetShaderObject(effect);

		shader_desc_ids_.fill(0);

		render_state_obj_ = inherit_pass->render_state_obj_;

		for (int type = 0; type < ShaderObject::ST_NumShaderTypes; ++ type)
		{
			ShaderDesc sd = effect.GetShaderDesc(inherit_pass->shader_desc_ids_[type]);
			if (!sd.func_name.empty())
			{
				sd.macros_hash = macros_hash;
				sd.tech_pass_type = (tech_index << 16) + (pass_index << 8) + type;
				shader_desc_ids_[type] = effect.AddShaderDesc(sd);
				
				auto const & tech = *effect.TechniqueByIndex(tech_index);
				shader_obj->AttachShader(static_cast<ShaderObject::ShaderType>(type),
					effect, tech, *this, shader_desc_ids_);
			}
		}

		shader_obj->LinkShaders(effect);

		is_validate_ = shader_obj->Validate();
	}
#endif

	bool RenderPass::StreamIn(RenderEffect& effect,
		ResIdentifierPtr const & res, uint32_t tech_index, uint32_t pass_index)
	{
		RenderFactory& rf = Context::Instance().RenderFactoryInstance();

		name_ = ReadShortString(res);
		name_hash_ = HashRange(name_.begin(), name_.end());

		uint8_t num_anno;
		res->read(&num_anno, sizeof(num_anno));
		if (num_anno > 0)
		{
			annotations_ = MakeSharedPtr<std::remove_reference<decltype(*annotations_)>::type>();
			annotations_->resize(num_anno);
			for (uint32_t i = 0; i < num_anno; ++ i)
			{
				RenderEffectAnnotationPtr annotation = MakeSharedPtr<RenderEffectAnnotation>();
				(*annotations_)[i] = annotation;
				
				annotation->StreamIn(res);
			}
		}

		uint8_t num_macro;
		res->read(&num_macro, sizeof(num_macro));
		if (num_macro > 0)
		{
			macros_ = MakeSharedPtr<std::remove_reference<decltype(*macros_)>::type>();
			macros_->resize(num_macro);
			for (uint32_t i = 0; i < num_macro; ++ i)
			{
				std::string name = ReadShortString(res);
				std::string value = ReadShortString(res);
				(*macros_)[i] = std::make_pair(name, value);
			}
		}

		RasterizerStateDesc rs_desc;
		DepthStencilStateDesc dss_desc;
		BlendStateDesc bs_desc;

		res->read(&rs_desc, sizeof(rs_desc));
		rs_desc.polygon_mode = LE2Native(rs_desc.polygon_mode);
		rs_desc.shade_mode = LE2Native(rs_desc.shade_mode);
		rs_desc.cull_mode = LE2Native(rs_desc.cull_mode);
		rs_desc.polygon_offset_factor = LE2Native(rs_desc.polygon_offset_factor);
		rs_desc.polygon_offset_units = LE2Native(rs_desc.polygon_offset_units);
		
		res->read(&dss_desc, sizeof(dss_desc));
		dss_desc.depth_func = LE2Native(dss_desc.depth_func);
		dss_desc.front_stencil_func = LE2Native(dss_desc.front_stencil_func);
		dss_desc.front_stencil_ref = LE2Native(dss_desc.front_stencil_ref);
		dss_desc.front_stencil_read_mask = LE2Native(dss_desc.front_stencil_read_mask);
		dss_desc.front_stencil_write_mask = LE2Native(dss_desc.front_stencil_write_mask);
		dss_desc.front_stencil_fail = LE2Native(dss_desc.front_stencil_fail);
		dss_desc.front_stencil_depth_fail = LE2Native(dss_desc.front_stencil_depth_fail);
		dss_desc.front_stencil_pass = LE2Native(dss_desc.front_stencil_pass);
		dss_desc.back_stencil_func = LE2Native(dss_desc.back_stencil_func);
		dss_desc.back_stencil_ref = LE2Native(dss_desc.back_stencil_ref);
		dss_desc.back_stencil_read_mask = LE2Native(dss_desc.back_stencil_read_mask);
		dss_desc.back_stencil_write_mask = LE2Native(dss_desc.back_stencil_write_mask);
		dss_desc.back_stencil_fail = LE2Native(dss_desc.back_stencil_fail);
		dss_desc.back_stencil_depth_fail = LE2Native(dss_desc.back_stencil_depth_fail);
		dss_desc.back_stencil_pass = LE2Native(dss_desc.back_stencil_pass);

		res->read(&bs_desc, sizeof(bs_desc));
		for (size_t i = 0; i < 4; ++ i)
		{
			bs_desc.blend_factor[i] = LE2Native(bs_desc.blend_factor[i]);
		}
		bs_desc.sample_mask = LE2Native(bs_desc.sample_mask);
		for (size_t i = 0; i < bs_desc.blend_op.size(); ++ i)
		{
			bs_desc.blend_op[i] = LE2Native(bs_desc.blend_op[i]);
			bs_desc.src_blend[i] = LE2Native(bs_desc.src_blend[i]);
			bs_desc.dest_blend[i] = LE2Native(bs_desc.dest_blend[i]);
			bs_desc.blend_op_alpha[i] = LE2Native(bs_desc.blend_op_alpha[i]);
			bs_desc.src_blend_alpha[i] = LE2Native(bs_desc.src_blend_alpha[i]);
			bs_desc.dest_blend_alpha[i] = LE2Native(bs_desc.dest_blend_alpha[i]);
		}
		
		render_state_obj_ = rf.MakeRenderStateObject(rs_desc, dss_desc, bs_desc);

		res->read(&shader_desc_ids_[0], shader_desc_ids_.size() * sizeof(shader_desc_ids_[0]));
		for (int i = 0; i < ShaderObject::ST_NumShaderTypes; ++ i)
		{
			shader_desc_ids_[i] = LE2Native(shader_desc_ids_[i]);
		}

		
		shader_obj_index_ = effect.AddShaderObject();
		auto const & shader_obj = this->GetShaderObject(effect);

		bool native_accepted = true;

		for (int type = 0; type < ShaderObject::ST_NumShaderTypes; ++ type)
		{
			ShaderDesc const & sd = effect.GetShaderDesc(shader_desc_ids_[type]);
			if (!sd.func_name.empty())
			{
				ShaderObject::ShaderType st = static_cast<ShaderObject::ShaderType>(type);

				bool this_native_accepted;
				if (sd.tech_pass_type != (tech_index << 16) + (pass_index << 8) + type)
				{
					auto const & tech = *effect.TechniqueByIndex(sd.tech_pass_type >> 16);
					auto const & pass = tech.Pass((sd.tech_pass_type >> 8) & 0xFF);
					shader_obj->AttachShader(st, effect, tech, pass, pass.GetShaderObject(effect));
					this_native_accepted = true;
				}
				else
				{
					this_native_accepted = shader_obj->StreamIn(res, static_cast<ShaderObject::ShaderType>(type),
						effect, shader_desc_ids_);
				}

				native_accepted &= this_native_accepted;
			}
		}

		shader_obj->LinkShaders(effect);

		is_validate_ = shader_obj->Validate();

		return native_accepted;
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderPass::StreamOut(RenderEffect const & effect, std::ostream& os, uint32_t tech_index, uint32_t pass_index) const
	{
		WriteShortString(os, name_);

		uint8_t num_anno;
		if (annotations_)
		{
			num_anno = static_cast<uint8_t>(annotations_->size());
		}
		else
		{
			num_anno = 0;
		}
		os.write(reinterpret_cast<char const *>(&num_anno), sizeof(num_anno));
		for (uint32_t i = 0; i < num_anno; ++ i)
		{
			RenderEffectAnnotationPtr annotation = MakeSharedPtr<RenderEffectAnnotation>();
			(*annotations_)[i] = annotation;
				
			annotation->StreamOut(os);
		}

		uint8_t num_macro;
		if (macros_)
		{
			num_macro = static_cast<uint8_t>(macros_->size());
		}
		else
		{
			num_macro = 0;
		}
		os.write(reinterpret_cast<char const *>(&num_macro), sizeof(num_macro));
		for (uint32_t i = 0; i < num_macro; ++ i)
		{
			WriteShortString(os, (*macros_)[i].first);
			WriteShortString(os, (*macros_)[i].second);
		}

		RasterizerStateDesc rs_desc = render_state_obj_->GetRasterizerStateDesc();
		DepthStencilStateDesc dss_desc = render_state_obj_->GetDepthStencilStateDesc();
		BlendStateDesc bs_desc = render_state_obj_->GetBlendStateDesc();

		rs_desc.polygon_mode = Native2LE(rs_desc.polygon_mode);
		rs_desc.shade_mode = Native2LE(rs_desc.shade_mode);
		rs_desc.cull_mode = Native2LE(rs_desc.cull_mode);
		rs_desc.polygon_offset_factor = Native2LE(rs_desc.polygon_offset_factor);
		rs_desc.polygon_offset_units = Native2LE(rs_desc.polygon_offset_units);
		os.write(reinterpret_cast<char const *>(&rs_desc), sizeof(rs_desc));
		
		dss_desc.depth_func = Native2LE(dss_desc.depth_func);
		dss_desc.front_stencil_func = Native2LE(dss_desc.front_stencil_func);
		dss_desc.front_stencil_ref = Native2LE(dss_desc.front_stencil_ref);
		dss_desc.front_stencil_read_mask = Native2LE(dss_desc.front_stencil_read_mask);
		dss_desc.front_stencil_write_mask = Native2LE(dss_desc.front_stencil_write_mask);
		dss_desc.front_stencil_fail = Native2LE(dss_desc.front_stencil_fail);
		dss_desc.front_stencil_depth_fail = Native2LE(dss_desc.front_stencil_depth_fail);
		dss_desc.front_stencil_pass = Native2LE(dss_desc.front_stencil_pass);
		dss_desc.back_stencil_func = Native2LE(dss_desc.back_stencil_func);
		dss_desc.back_stencil_ref = Native2LE(dss_desc.back_stencil_ref);
		dss_desc.back_stencil_read_mask = Native2LE(dss_desc.back_stencil_read_mask);
		dss_desc.back_stencil_write_mask = Native2LE(dss_desc.back_stencil_write_mask);
		dss_desc.back_stencil_fail = Native2LE(dss_desc.back_stencil_fail);
		dss_desc.back_stencil_depth_fail = Native2LE(dss_desc.back_stencil_depth_fail);
		dss_desc.back_stencil_pass = Native2LE(dss_desc.back_stencil_pass);
		os.write(reinterpret_cast<char const *>(&dss_desc), sizeof(dss_desc));

		for (size_t i = 0; i < 4; ++ i)
		{
			bs_desc.blend_factor[i] = Native2LE(bs_desc.blend_factor[i]);
		}
		bs_desc.sample_mask = Native2LE(bs_desc.sample_mask);
		for (size_t i = 0; i < bs_desc.blend_op.size(); ++ i)
		{
			bs_desc.blend_op[i] = Native2LE(bs_desc.blend_op[i]);
			bs_desc.src_blend[i] = Native2LE(bs_desc.src_blend[i]);
			bs_desc.dest_blend[i] = Native2LE(bs_desc.dest_blend[i]);
			bs_desc.blend_op_alpha[i] = Native2LE(bs_desc.blend_op_alpha[i]);
			bs_desc.src_blend_alpha[i] = Native2LE(bs_desc.src_blend_alpha[i]);
			bs_desc.dest_blend_alpha[i] = Native2LE(bs_desc.dest_blend_alpha[i]);
		}		
		os.write(reinterpret_cast<char const *>(&bs_desc), sizeof(bs_desc));

		for (uint32_t i = 0; i < shader_desc_ids_.size(); ++ i)
		{
			uint32_t tmp = Native2LE(shader_desc_ids_[i]);
			os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
		}

		for (int type = 0; type < ShaderObject::ST_NumShaderTypes; ++ type)
		{
			ShaderDesc const & sd = effect.GetShaderDesc(shader_desc_ids_[type]);
			if (!sd.func_name.empty())
			{
				if (sd.tech_pass_type == (tech_index << 16) + (pass_index << 8) + type)
				{
					this->GetShaderObject(effect)->StreamOut(os, static_cast<ShaderObject::ShaderType>(type));
				}
			}
		}
	}
#endif

	void RenderPass::Bind(RenderEffect const & effect) const
	{
		RenderEngine& render_eng = Context::Instance().RenderFactoryInstance().RenderEngineInstance();
		render_eng.SetStateObject(render_state_obj_);

		this->GetShaderObject(effect)->Bind();
	}

	void RenderPass::Unbind(RenderEffect const & effect) const
	{
		this->GetShaderObject(effect)->Unbind();
	}


#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectConstantBuffer::Load(std::string const & name)
	{
		name_ = MakeSharedPtr<std::remove_reference<decltype(*name_)>::type>();
		name_->first = name;
		name_->second = HashRange(name_->first.begin(), name_->first.end());
		param_indices_ = MakeSharedPtr<std::remove_reference<decltype(*param_indices_)>::type>();
	}
#endif

	void RenderEffectConstantBuffer::StreamIn(ResIdentifierPtr const & res)
	{
		name_ = MakeSharedPtr<std::remove_reference<decltype(*name_)>::type>();
		name_->first = ReadShortString(res);
		name_->second = HashRange(name_->first.begin(), name_->first.end());
		param_indices_ = MakeSharedPtr<std::remove_reference<decltype(*param_indices_)>::type>();

		uint16_t len;
		res->read(&len, sizeof(len));
		len = LE2Native(len);
		param_indices_->resize(len);
		res->read(&(*param_indices_)[0], len * sizeof((*param_indices_)[0]));
		for (uint32_t i = 0; i < len; ++ i)
		{
			(*param_indices_)[i] = LE2Native((*param_indices_)[i]);
		}
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectConstantBuffer::StreamOut(std::ostream& os) const
	{
		WriteShortString(os, name_->first);

		uint16_t len = Native2LE(static_cast<uint16_t>(param_indices_->size()));
		os.write(reinterpret_cast<char const *>(&len), sizeof(len));
		for (size_t i = 0; i < param_indices_->size(); ++ i)
		{
			uint32_t tmp = Native2LE((*param_indices_)[i]);
			os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
		}
	}
#endif

	std::unique_ptr<RenderEffectConstantBuffer> RenderEffectConstantBuffer::Clone(RenderEffect& src_effect, RenderEffect& dst_effect)
	{
		auto ret = MakeUniquePtr<RenderEffectConstantBuffer>();

		ret->name_ = name_;
		ret->param_indices_ = param_indices_;
		ret->buff_ = buff_;
		ret->Resize(static_cast<uint32_t>(buff_.size()));

		for (size_t i = 0; i < param_indices_->size(); ++ i)
		{
			RenderEffectParameter* src_param = src_effect.ParameterByIndex((*param_indices_)[i]);
			if (src_param->InCBuffer())
			{
				RenderEffectParameter* dst_param = dst_effect.ParameterByIndex((*param_indices_)[i]);
				dst_param->RebindToCBuffer(*ret);
			}
		}

		return ret;
	}

	void RenderEffectConstantBuffer::AddParameter(uint32_t index)
	{
		param_indices_->push_back(index);
	}

	void RenderEffectConstantBuffer::Resize(uint32_t size)
	{
		buff_.resize(size);
		if (size > 0)
		{
			if (!hw_buff_ || (size > hw_buff_->Size()))
			{
				RenderFactory& rf = Context::Instance().RenderFactoryInstance();
				hw_buff_ = rf.MakeConstantBuffer(BU_Dynamic, 0, size, nullptr);
			}
		}

		dirty_ = true;
	}

	void RenderEffectConstantBuffer::Update()
	{
		if (dirty_)
		{
			hw_buff_->UpdateSubresource(0, static_cast<uint32_t>(buff_.size()), &buff_[0]);

			dirty_ = false;
		}
	}

	void RenderEffectConstantBuffer::BindHWBuff(GraphicsBufferPtr const & buff)
	{
		hw_buff_ = buff;
		buff_.resize(buff->Size());
	}


#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectParameter::Load(XMLNodePtr const & node)
	{
		type_ = type_define::instance().TypeCode(node->Attrib("type")->ValueString());
		name_ = MakeSharedPtr<std::remove_reference<decltype(*name_)>::type>();
		name_->first = node->Attrib("name")->ValueString();
		name_->second = HashRange(name_->first.begin(), name_->first.end());

		XMLAttributePtr attr = node->Attrib("semantic");
		if (attr)
		{
			semantic_ = MakeSharedPtr<std::remove_reference<decltype(*semantic_)>::type>();
			semantic_->first = attr->ValueString();
			semantic_->second = HashRange(semantic_->first.begin(), semantic_->first.end());
		}

		uint32_t as;
		attr = node->Attrib("array_size");
		if (attr)
		{
			array_size_ = MakeSharedPtr<std::string>(attr->ValueString());

			if (!attr->TryConvert(as))
			{
				as = 1;  // dummy array size
			}
		}
		else
		{
			as = 0;
		}
		var_ = read_var(node, type_, as);

		{
			XMLNodePtr anno_node = node->FirstNode("annotation");
			if (anno_node)
			{
				annotations_ = MakeSharedPtr<std::remove_reference<decltype(*annotations_)>::type>();
				for (; anno_node; anno_node = anno_node->NextSibling("annotation"))
				{
					annotations_->push_back(MakeUniquePtr<RenderEffectAnnotation>());
					annotations_->back()->Load(anno_node);
				}
			}
		}

		if (annotations_ && ((REDT_texture1D == type_) || (REDT_texture2D == type_) || (REDT_texture3D == type_) || (REDT_textureCUBE == type_)
			|| (REDT_texture1DArray == type_) || (REDT_texture2DArray == type_) || (REDT_texture3DArray == type_) || (REDT_textureCUBEArray == type_)))
		{
			for (size_t i = 0; i < annotations_->size(); ++ i)
			{
				if (REDT_string == (*annotations_)[i]->Type())
				{
					if ("SasResourceAddress" == (*annotations_)[i]->Name())
					{
						std::string val;
						(*annotations_)[i]->Value(val);

						if (ResLoader::Instance().Locate(val).empty())
						{
							LogError("%s NOT found", val.c_str());
						}
						else
						{
							*var_ = SyncLoadTexture(val, EAH_GPU_Read | EAH_Immutable);
						}
					}
				}
			}
		}
	}
#endif

	void RenderEffectParameter::StreamIn(ResIdentifierPtr const & res)
	{
		res->read(&type_, sizeof(type_));
		type_ = LE2Native(type_);
		name_ = MakeSharedPtr<std::remove_reference<decltype(*name_)>::type>();
		name_->first = ReadShortString(res);
		name_->second = HashRange(name_->first.begin(), name_->first.end());

		std::string sem = ReadShortString(res);
		if (!sem.empty())
		{
			semantic_ = MakeSharedPtr<std::remove_reference<decltype(*semantic_)>::type>();
			semantic_->first = sem;
			semantic_->second = HashRange(sem.begin(), sem.end());
		}

		uint32_t as;
		std::string as_str = ReadShortString(res);
		if (as_str.empty())
		{
			as = 0;
		}
		else
		{
			array_size_ = MakeSharedPtr<std::string>(as_str);

			if (!boost::conversion::try_lexical_convert(as_str, as))
			{
				as = 1;  // dummy array size
			}
		}
		var_ = stream_in_var(res, type_, as);

		uint8_t num_anno;
		res->read(&num_anno, sizeof(num_anno));
		if (num_anno > 0)
		{
			annotations_ = MakeSharedPtr<std::remove_reference<decltype(*annotations_)>::type>();
			annotations_->resize(num_anno);
			for (uint32_t i = 0; i < num_anno; ++ i)
			{
				(*annotations_)[i] = MakeUniquePtr<RenderEffectAnnotation>();				
				(*annotations_)[i]->StreamIn(res);
			}
		}

		if (annotations_ && ((REDT_texture1D == type_) || (REDT_texture2D == type_) || (REDT_texture3D == type_) || (REDT_textureCUBE == type_)
			|| (REDT_texture1DArray == type_) || (REDT_texture2DArray == type_) || (REDT_texture3DArray == type_) || (REDT_textureCUBEArray == type_)))
		{
			for (size_t i = 0; i < annotations_->size(); ++ i)
			{
				if (REDT_string == (*annotations_)[i]->Type())
				{
					if ("SasResourceAddress" == (*annotations_)[i]->Name())
					{
						std::string val;
						(*annotations_)[i]->Value(val);

						if (ResLoader::Instance().Locate(val).empty())
						{
							LogError("%s NOT found", val.c_str());
						}
						else
						{
							*var_ = SyncLoadTexture(val, EAH_GPU_Read | EAH_Immutable);
						}
					}
				}
			}
		}
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderEffectParameter::StreamOut(std::ostream& os) const
	{
		uint32_t t = Native2LE(type_);
		os.write(reinterpret_cast<char const *>(&t), sizeof(t));
		WriteShortString(os, name_->first);
		if (semantic_)
		{
			WriteShortString(os, semantic_->first);
		}
		else
		{
			uint8_t len = 0;
			os.write(reinterpret_cast<char const *>(&len), sizeof(len));
		}

		if (array_size_)
		{
			WriteShortString(os, *array_size_);
		}
		else
		{
			uint8_t len = 0;
			os.write(reinterpret_cast<char const *>(&len), sizeof(len));
		}
		uint32_t as;
		if (array_size_)
		{
			if (!boost::conversion::try_lexical_convert(*array_size_, as))
			{
				as = 1;  // dummy array size
			}
		}
		else
		{
			as = 0;
		}
		stream_out_var(os, *var_, type_, as);

		uint8_t num_anno;
		if (annotations_)
		{
			num_anno = static_cast<uint8_t>(annotations_->size());
		}
		else
		{
			num_anno = 0;
		}
		os.write(reinterpret_cast<char const *>(&num_anno), sizeof(num_anno));
		for (uint32_t i = 0; i < num_anno; ++ i)
		{
			(*annotations_)[i]->StreamOut(os);
		}
	}
#endif

	std::unique_ptr<RenderEffectParameter> RenderEffectParameter::Clone()
	{
		std::unique_ptr<RenderEffectParameter> ret = MakeUniquePtr<RenderEffectParameter>();

		ret->name_ = name_;
		ret->semantic_ = semantic_;

		ret->type_ = type_;
		ret->var_ = var_->Clone();
		ret->array_size_ = array_size_;

		ret->annotations_ = annotations_;

		return ret;
	}

	std::string const & RenderEffectParameter::Semantic() const
	{
		if (this->HasSemantic())
		{
			return semantic_->first;
		}
		else
		{
			static std::string empty("");
			return empty;
		}
	}

	size_t RenderEffectParameter::SemanticHash() const
	{
		return this->HasSemantic() ? semantic_->second : 0;
	}

	void RenderEffectParameter::BindToCBuffer(RenderEffectConstantBuffer& cbuff, uint32_t offset, uint32_t stride)
	{
		cbuff_ = &cbuff;
		var_->BindToCBuffer(cbuff, offset, stride);
	}

	void RenderEffectParameter::RebindToCBuffer(RenderEffectConstantBuffer& cbuff)
	{
		cbuff_ = &cbuff;
		var_->RebindToCBuffer(cbuff);
	}


#if KLAYGE_IS_DEV_PLATFORM
	void RenderShaderFragment::Load(XMLNodePtr const & node)
	{
		type_ = ShaderObject::ST_NumShaderTypes;
		XMLAttributePtr attr = node->Attrib("type");
		if (attr)
		{
			std::string type_str = attr->ValueString();
			size_t const type_str_hash = RT_HASH(type_str.c_str());
			if (CT_HASH("vertex_shader") == type_str_hash)
			{
				type_ = ShaderObject::ST_VertexShader;
			}
			else if (CT_HASH("pixel_shader") == type_str_hash)
			{
				type_ = ShaderObject::ST_PixelShader;
			}
			else if (CT_HASH("geometry_shader") == type_str_hash)
			{
				type_ = ShaderObject::ST_GeometryShader;
			}
			else if (CT_HASH("compute_shader") == type_str_hash)
			{
				type_ = ShaderObject::ST_ComputeShader;
			}
			else if (CT_HASH("hull_shader") == type_str_hash)
			{
				type_ = ShaderObject::ST_HullShader;
			}
			else
			{
				BOOST_ASSERT("domain_shader" == type_str);
				type_ = ShaderObject::ST_DomainShader;
			}
		}
		
		ver_ = ShaderModel(0, 0);
		attr = node->Attrib("major_version");
		if (attr)
		{
			uint8_t minor_ver = 0;
			XMLAttributePtr minor_attr = node->Attrib("minor_version");
			if (minor_attr)
			{
				minor_ver = static_cast<uint8_t>(minor_attr->ValueInt());
			}
			ver_ = ShaderModel(static_cast<uint8_t>(attr->ValueInt()), minor_ver);
		}
		else
		{
			attr = node->Attrib("version");
			if (attr)
			{
				ver_ = ShaderModel(static_cast<uint8_t>(attr->ValueInt()), 0);
			}
		}

		for (XMLNodePtr shader_text_node = node->FirstNode(); shader_text_node; shader_text_node = shader_text_node->NextSibling())
		{
			if ((XNT_Comment == shader_text_node->Type()) || (XNT_CData == shader_text_node->Type()))
			{
				str_ += shader_text_node->ValueString();
			}
		}
	}
#endif

	void RenderShaderFragment::StreamIn(ResIdentifierPtr const & res)
	{
		res->read(&type_, sizeof(type_));
		type_ = LE2Native(type_);
		res->read(&ver_, sizeof(ver_));

		uint32_t len;
		res->read(&len, sizeof(len));
		len = LE2Native(len);
		str_.resize(len);
		res->read(&str_[0], len * sizeof(str_[0]));
	}

#if KLAYGE_IS_DEV_PLATFORM
	void RenderShaderFragment::StreamOut(std::ostream& os) const
	{
		uint32_t tmp;
		tmp = Native2LE(type_);
		os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
		os.write(reinterpret_cast<char const *>(&ver_), sizeof(ver_));

		uint32_t len = static_cast<uint32_t>(str_.size());
		tmp = Native2LE(len);
		os.write(reinterpret_cast<char const *>(&tmp), sizeof(tmp));
		os.write(&str_[0], len * sizeof(str_[0]));
	}
#endif


	RenderVariable::RenderVariable()
	{
	}

	RenderVariable::~RenderVariable()
	{
	}

	RenderVariable& RenderVariable::operator=(bool const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(uint32_t const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(int32_t const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(float const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(uint2 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(uint3 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(uint4 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(int2 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(int3 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(int4 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(float2 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(float3 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(float4 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(float4x4 const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(TexturePtr const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(TextureSubresource const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(SamplerStateObjectPtr const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(GraphicsBufferPtr const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::string const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(ShaderDesc const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<bool> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<uint32_t> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<int32_t> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<float> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<uint2> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<uint3> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<uint4> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<int2> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<int3> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<int4> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<float2> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<float3> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<float4> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	RenderVariable& RenderVariable::operator=(std::vector<float4x4> const & /*value*/)
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(bool& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(uint32_t& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(int32_t& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(float& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(uint2& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(uint3& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(uint4& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(int2& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(int3& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(int4& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(float2& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(float3& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(float4& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(float4x4& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(TexturePtr& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(TextureSubresource& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(SamplerStateObjectPtr& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(GraphicsBufferPtr& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::string& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(ShaderDesc& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<bool>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<uint32_t>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<int32_t>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<float>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<uint2>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<uint3>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<uint4>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<int2>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<int3>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<int4>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<float2>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<float3>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<float4>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::Value(std::vector<float4x4>& /*value*/) const
	{
		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::BindToCBuffer(RenderEffectConstantBuffer& cbuff, uint32_t offset,
			uint32_t stride)
	{
		KFL_UNUSED(cbuff);
		KFL_UNUSED(offset);
		KFL_UNUSED(stride);

		KFL_UNREACHABLE("Can't be called");
	}

	void RenderVariable::RebindToCBuffer(RenderEffectConstantBuffer& cbuff)
	{
		KFL_UNUSED(cbuff);
		
		KFL_UNREACHABLE("Can't be called");
	}

	std::unique_ptr<RenderVariable> RenderVariableFloat4x4::Clone()
	{
		auto ret = MakeUniquePtr<RenderVariableFloat4x4>();
		ret->in_cbuff_ = in_cbuff_;
		if (in_cbuff_)
		{
			ret->data_ = data_;
		}
		float4x4 val;
		this->Value(val);
		*ret = val;
		return std::move(ret);
	}

	RenderVariable& RenderVariableFloat4x4::operator=(float4x4 const & value)
	{
		return RenderVariableConcrete<float4x4>::operator=(MathLib::transpose(value));
	}

	void RenderVariableFloat4x4::Value(float4x4& val) const
	{
		RenderVariableConcrete<float4x4>::Value(val);
		val = MathLib::transpose(val);
	}

	std::unique_ptr<RenderVariable> RenderVariableFloat4x4Array::Clone()
	{
		auto ret = MakeUniquePtr<RenderVariableFloat4x4Array>();
		if (in_cbuff_)
		{
			if (!ret->in_cbuff_)
			{
				ret->RetriveT().~vector();
			}
			ret->data_ = data_;
		}
		ret->in_cbuff_ = in_cbuff_;
		std::vector<float4x4> val;
		this->Value(val);
		*ret = val;
		return std::move(ret);
	}

	RenderVariable& RenderVariableFloat4x4Array::operator=(std::vector<float4x4> const & value)
	{
		if (in_cbuff_)
		{
			float4x4* target = data_.cbuff_desc.cbuff->VariableInBuff<float4x4>(data_.cbuff_desc.offset);

			size_ = static_cast<uint32_t>(value.size());
			for (size_t i = 0; i < value.size(); ++ i)
			{
				target[i] = MathLib::transpose(value[i]);
			}

			data_.cbuff_desc.cbuff->Dirty(true);
		}
		else
		{
			this->RetriveT() = value;
		}
		return *this;
	}

	void RenderVariableFloat4x4Array::Value(std::vector<float4x4>& val) const
	{
		if (in_cbuff_)
		{
			float4x4 const * src = data_.cbuff_desc.cbuff->VariableInBuff<float4x4>(data_.cbuff_desc.offset);

			val.resize(size_);
			for (size_t i = 0; i < size_; ++ i)
			{
				val[i] = MathLib::transpose(src[i]);
			}
		}
		else
		{
			val = this->RetriveT();
		}
	}

	std::unique_ptr<RenderVariable> RenderVariableTexture::Clone()
	{
		auto ret = MakeUniquePtr<RenderVariableTexture>();
		TexturePtr val;
		this->Value(val);
		*ret = val;
		std::string elem_type;
		this->Value(elem_type);
		*ret = elem_type;
		return std::move(ret);
	}

	RenderVariable& RenderVariableTexture::operator=(TexturePtr const & value)
	{
		uint32_t array_size = 1;
		uint32_t mipmap = 1;
		if (value)
		{
			array_size = value->ArraySize();
			mipmap = value->NumMipMaps();
		}
		val_ = TextureSubresource(value, 0, array_size, 0, mipmap);
		return *this;
	}

	RenderVariable& RenderVariableTexture::operator=(TextureSubresource const & value)
	{
		val_ = value;
		return *this;
	}

	void RenderVariableTexture::Value(TexturePtr& val) const
	{
		if (val_.tex)
		{
			val_.num_items = val_.tex->ArraySize();
			val_.num_levels = val_.tex->NumMipMaps();
		}
		val = val_.tex;
	}

	void RenderVariableTexture::Value(TextureSubresource& val) const
	{
		if (val_.tex)
		{
			val_.num_items = val_.tex->ArraySize();
			val_.num_levels = val_.tex->NumMipMaps();
		}
		val = val_;
	}

	RenderVariable& RenderVariableTexture::operator=(std::string const & value)
	{
		elem_type_ = value;
		return *this;
	}

	void RenderVariableTexture::Value(std::string& val) const
	{
		val = elem_type_;
	}


	std::unique_ptr<RenderVariable> RenderVariableBuffer::Clone()
	{
		auto ret = MakeUniquePtr<RenderVariableBuffer>();
		GraphicsBufferPtr val;
		this->Value(val);
		*ret = val;
		std::string elem_type;
		this->Value(elem_type);
		*ret = elem_type;
		return std::move(ret);
	}

	RenderVariable& RenderVariableBuffer::operator=(GraphicsBufferPtr const & value)
	{
		val_ = value;
		return *this;
	}

	void RenderVariableBuffer::Value(GraphicsBufferPtr& val) const
	{
		val = val_;
	}

	RenderVariable& RenderVariableBuffer::operator=(std::string const & value)
	{
		elem_type_ = value;
		return *this;
	}

	void RenderVariableBuffer::Value(std::string& val) const
	{
		val = elem_type_;
	}


	std::unique_ptr<RenderVariable> RenderVariableByteAddressBuffer::Clone()
	{
		auto ret = MakeUniquePtr<RenderVariableByteAddressBuffer>();
		GraphicsBufferPtr val;
		this->Value(val);
		*ret = val;
		std::string elem_type;
		this->Value(elem_type);
		*ret = elem_type;
		return std::move(ret);
	}

	RenderVariable& RenderVariableByteAddressBuffer::operator=(GraphicsBufferPtr const & value)
	{
		val_ = value;
		return *this;
	}

	void RenderVariableByteAddressBuffer::Value(GraphicsBufferPtr& val) const
	{
		val = val_;
	}

	RenderVariable& RenderVariableByteAddressBuffer::operator=(std::string const & value)
	{
		elem_type_ = value;
		return *this;
	}

	void RenderVariableByteAddressBuffer::Value(std::string& val) const
	{
		val = elem_type_;
	}


	RenderEffectPtr SyncLoadRenderEffect(std::string const & effect_name)
	{
		return ResLoader::Instance().SyncQueryT<RenderEffect>(MakeSharedPtr<EffectLoadingDesc>(effect_name));
	}

	RenderEffectPtr ASyncLoadRenderEffect(std::string const & effect_name)
	{
		// TODO: Make it really async
		return ResLoader::Instance().SyncQueryT<RenderEffect>(MakeSharedPtr<EffectLoadingDesc>(effect_name));
	}
}
