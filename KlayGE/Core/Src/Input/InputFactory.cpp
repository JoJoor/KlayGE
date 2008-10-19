// InputFactory.cpp
// KlayGE 输入引擎抽象工厂 实现文件
// Ver 3.1.0
// 版权所有(C) 龚敏敏, 2003-2005
// Homepage: http://klayge.sourceforge.net
//
// 3.1.0
// 初次建立 (2005.10.29)
//
// 修改记录
/////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/Util.hpp>
#include <KlayGE/Input.hpp>

#include <string>

#include <KlayGE/InputFactory.hpp>

namespace KlayGE
{
	class NullInputFactory : public InputFactory
	{
	public:
		std::wstring const & Name() const
		{
			static std::wstring const name(L"Null Input Factory");
			return name;
		}

		InputEnginePtr DoMakeInputEngine()
		{
			return InputEngine::NullObject();
		}
	};

	InputFactoryPtr InputFactory::NullObject()
	{
		static InputFactoryPtr obj = MakeSharedPtr<NullInputFactory>();
		return obj;
	}

	InputEngine& InputFactory::InputEngineInstance()
	{
		if (!ie_)
		{
			ie_ = this->DoMakeInputEngine();
		}

		return *ie_;
	}
}
