// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal/InputMappingUETranslator.h"

#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

void InputMappingUETranslator::initialize(UObject* outer, const dInput::MappingContext& context)
{
	m_mappingContext = NewObject<UInputMappingContext>(outer);
	m_actionMap.clear();

	// Create UInputAction* for each unique ActionDescriptor
	for (const auto& actionMapping : context.actionMappings)
	{
		const dInput::ActionDescriptor* desc = actionMapping.action;
		if (m_actionMap.find(desc) == m_actionMap.end())
		{
			UInputAction* action = NewObject<UInputAction>(outer);

			switch (desc->valueType)
			{
			case dInput::ActionValueType::Boolean: action->ValueType = EInputActionValueType::Boolean; break;
			case dInput::ActionValueType::Axis1D:  action->ValueType = EInputActionValueType::Axis1D;  break;
			case dInput::ActionValueType::Axis2D:  action->ValueType = EInputActionValueType::Axis2D;  break;
			case dInput::ActionValueType::Axis3D:  action->ValueType = EInputActionValueType::Axis3D;  break;
			}

			m_actionMap[desc] = action;
		}
	}

	// Map keys with modifiers
	for (const auto& actionMapping : context.actionMappings)
	{
		UInputAction* action = m_actionMap[actionMapping.action];

		for (const auto& binding : actionMapping.bindings)
		{
			FKey key = toUnrealKey(binding.key);
			FEnhancedActionKeyMapping& mapping = m_mappingContext->MapKey(action, key);

			switch (binding.modifier)
			{
			case dInput::KeyModifier::Negate:
				mapping.Modifiers.Add(NewObject<UInputModifierNegate>(outer));
				break;
			case dInput::KeyModifier::SwizzleAxis:
				mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(outer));
				break;
			case dInput::KeyModifier::NegateAndSwizzle:
				mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(outer));
				mapping.Modifiers.Add(NewObject<UInputModifierNegate>(outer));
				break;
			case dInput::KeyModifier::None:
				break;
			}
		}
	}
}

void InputMappingUETranslator::addToSubsystem(UEnhancedInputLocalPlayerSubsystem* subsystem, int32_t priority)
{
	if (subsystem && m_mappingContext)
	{
		subsystem->AddMappingContext(m_mappingContext, static_cast<int32>(priority));
	}
}

UInputAction* InputMappingUETranslator::getAction(const dInput::ActionDescriptor& descriptor) const
{
	auto it = m_actionMap.find(&descriptor);
	if (it != m_actionMap.end())
	{
		return it->second;
	}
	return nullptr;
}

FKey InputMappingUETranslator::toUnrealKey(dInput::KeyId keyId)
{
	switch (keyId)
	{
	// Keyboard
	case dInput::KeyId::Key_A:          return EKeys::A;
	case dInput::KeyId::Key_B:          return EKeys::B;
	case dInput::KeyId::Key_C:          return EKeys::C;
	case dInput::KeyId::Key_D:          return EKeys::D;
	case dInput::KeyId::Key_E:          return EKeys::E;
	case dInput::KeyId::Key_F:          return EKeys::F;
	case dInput::KeyId::Key_G:          return EKeys::G;
	case dInput::KeyId::Key_H:          return EKeys::H;
	case dInput::KeyId::Key_I:          return EKeys::I;
	case dInput::KeyId::Key_J:          return EKeys::J;
	case dInput::KeyId::Key_K:          return EKeys::K;
	case dInput::KeyId::Key_L:          return EKeys::L;
	case dInput::KeyId::Key_M:          return EKeys::M;
	case dInput::KeyId::Key_N:          return EKeys::N;
	case dInput::KeyId::Key_O:          return EKeys::O;
	case dInput::KeyId::Key_P:          return EKeys::P;
	case dInput::KeyId::Key_Q:          return EKeys::Q;
	case dInput::KeyId::Key_R:          return EKeys::R;
	case dInput::KeyId::Key_S:          return EKeys::S;
	case dInput::KeyId::Key_T:          return EKeys::T;
	case dInput::KeyId::Key_U:          return EKeys::U;
	case dInput::KeyId::Key_V:          return EKeys::V;
	case dInput::KeyId::Key_W:          return EKeys::W;
	case dInput::KeyId::Key_X:          return EKeys::X;
	case dInput::KeyId::Key_Y:          return EKeys::Y;
	case dInput::KeyId::Key_Z:          return EKeys::Z;
	case dInput::KeyId::Key_Space:      return EKeys::SpaceBar;
	case dInput::KeyId::Key_LeftShift:  return EKeys::LeftShift;
	case dInput::KeyId::Key_LeftControl: return EKeys::LeftControl;
	case dInput::KeyId::Key_LeftAlt:    return EKeys::LeftAlt;
	case dInput::KeyId::Key_1: return EKeys::One;
	case dInput::KeyId::Key_2: return EKeys::Two;
	case dInput::KeyId::Key_3: return EKeys::Three;
	case dInput::KeyId::Key_4: return EKeys::Four;
	case dInput::KeyId::Key_5: return EKeys::Five;
	case dInput::KeyId::Key_6: return EKeys::Six;
	case dInput::KeyId::Key_7: return EKeys::Seven;
	case dInput::KeyId::Key_8: return EKeys::Eight;
	case dInput::KeyId::Key_9: return EKeys::Nine;
	case dInput::KeyId::Key_0: return EKeys::Zero;

	// Mouse
	case dInput::KeyId::Mouse_X:        return EKeys::MouseX;
	case dInput::KeyId::Mouse_Y:        return EKeys::MouseY;
	case dInput::KeyId::Mouse_XY:       return EKeys::Mouse2D;
	case dInput::KeyId::Mouse_Left:     return EKeys::LeftMouseButton;
	case dInput::KeyId::Mouse_Right:    return EKeys::RightMouseButton;
	case dInput::KeyId::Mouse_Middle:   return EKeys::MiddleMouseButton;

	// Gamepad
	case dInput::KeyId::Gamepad_LeftStick_XY:      return EKeys::Gamepad_Left2D;
	case dInput::KeyId::Gamepad_RightStick_XY:     return EKeys::Gamepad_Right2D;
	case dInput::KeyId::Gamepad_FaceBottom:         return EKeys::Gamepad_FaceButton_Bottom;
	case dInput::KeyId::Gamepad_FaceRight:          return EKeys::Gamepad_FaceButton_Right;
	case dInput::KeyId::Gamepad_FaceLeft:           return EKeys::Gamepad_FaceButton_Left;
	case dInput::KeyId::Gamepad_FaceTop:            return EKeys::Gamepad_FaceButton_Top;
	case dInput::KeyId::Gamepad_LeftTrigger:        return EKeys::Gamepad_LeftTrigger;
	case dInput::KeyId::Gamepad_RightTrigger:       return EKeys::Gamepad_RightTrigger;
	case dInput::KeyId::Gamepad_LeftTriggerAxis:    return EKeys::Gamepad_LeftTriggerAxis;
	case dInput::KeyId::Gamepad_RightTriggerAxis:   return EKeys::Gamepad_RightTriggerAxis;
	case dInput::KeyId::Gamepad_LeftShoulder:       return EKeys::Gamepad_LeftShoulder;
	case dInput::KeyId::Gamepad_RightShoulder:      return EKeys::Gamepad_RightShoulder;

	default: return EKeys::Invalid;
	}
}
