#include "Combat/DADamageTypes.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Data_Damage, "Data.Damage");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Data_DamageChannel_Kinetic, "Data.Damage.Channel.Kinetic");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Data_DamageChannel_Thermal, "Data.Damage.Channel.Thermal");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Data_DamageChannel_Arc, "Data.Damage.Channel.Arc");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Data_DamageChannel_Bio, "Data.Damage.Channel.Bio");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Data_DamageChannel_Cyber, "Data.Damage.Channel.Cyber");
UE_DEFINE_GAMEPLAY_TAG(TAG_DA_Data_DamageChannel_Structural, "Data.Damage.Channel.Structural");

const FGameplayTag& DADamageTags::Damage() { return TAG_DA_Data_Damage; }
const FGameplayTag& DADamageTags::Kinetic() { return TAG_DA_Data_DamageChannel_Kinetic; }
const FGameplayTag& DADamageTags::Thermal() { return TAG_DA_Data_DamageChannel_Thermal; }
const FGameplayTag& DADamageTags::Arc() { return TAG_DA_Data_DamageChannel_Arc; }
const FGameplayTag& DADamageTags::Bio() { return TAG_DA_Data_DamageChannel_Bio; }
const FGameplayTag& DADamageTags::Cyber() { return TAG_DA_Data_DamageChannel_Cyber; }
const FGameplayTag& DADamageTags::Structural() { return TAG_DA_Data_DamageChannel_Structural; }
