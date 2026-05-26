// SPDX-License-Identifier: MPL-2.0

#include "SimulationTimeContextUImpl.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"


// AAuthoritySimulationTimeManagerUImpl* AAuthoritySimulationTimeManagerUImpl::s_instances[] = {nullptr, nullptr};

// AAuthoritySimulationTimeManagerUImpl::AAuthoritySimulationTimeManagerUImpl()
// {
//     bReplicates = true;
// 	this->NetUpdateFrequency = 100.0f; // Higher = more updates per second
// 	this->MinNetUpdateFrequency = 100.0f;
// 	this->bAlwaysRelevant = true; // Always relevant for network update
// 	this->NetCullDistanceSquared = 10000.f * 10000.f; // Distance at which the character is culled from the network updates
// }

// AAuthoritySimulationTimeManagerUImpl::~AAuthoritySimulationTimeManagerUImpl()
// {
// 	if (GetWorld() != nullptr)
// 		GetWorld()->GetPhysicsScene()->OnPhysSceneStep.RemoveAll(this);

// 	if (GetNetMode() == NM_DedicatedServer)
// 	{
// 		s_instances[0] = nullptr;
// 	}
// 	else if (GetNetMode() == NM_Client)
// 	{
// 		s_instances[1] = nullptr;
// 	}
// 	else
// 	{
// 		//throw std::invalid_argument("baad");
// 	}
// }

// void AAuthoritySimulationTimeManagerUImpl::BeginPlay()
// {
// 	Super::BeginPlay();

// 	if (GetNetMode() == NM_DedicatedServer)
// 	{
// 		if (s_instances[0] != nullptr)
// 		{
// 			throw std::invalid_argument("baad");
// 		}
// 		s_instances[0] = this;
// 		m_timingInfo.emplace(false);
// 	}
// 	else if (GetNetMode() == NM_Client)
// 	{
// 		if (s_instances[1] != nullptr)
// 		{
// 			throw std::invalid_argument("baad");
// 		}
// 		s_instances[1] = this;
// 		m_timingInfo.emplace(true);

// 	}
// 	else
// 	{
// 		throw std::invalid_argument("baad");
// 	}


// 	GetWorld()->GetPhysicsScene()->OnPhysSceneStep.AddUObject(this, &AAuthoritySimulationTimeManagerUImpl::OnPhysicsStep);
// }

// void AAuthoritySimulationTimeManagerUImpl::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
// {
//     Super::GetLifetimeReplicatedProps(OutLifetimeProps);
//     DOREPLIFETIME(AAuthoritySimulationTimeManagerUImpl, m_syncedBuffer);
// }

// void AAuthoritySimulationTimeManagerUImpl::OnPhysicsStep(FPhysScene* Scene, float DeltaTime)
// {
//     m_timingInfo->advanceTick();
//     if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_ListenServer)
//         AuthoritySimulationTimeManager::writeStateToSyncedBuffer(*m_timingInfo, m_syncedBuffer, 0);
// }

// void AAuthoritySimulationTimeManagerUImpl::OnRep_TimingInfo()
// {
//     AuthoritySimulationTimeManager timingInfo(true);
//     AuthoritySimulationTimeManager::readStateFromSyncedBuffer(timingInfo, m_syncedBuffer, 0);

//     if (timingInfo.getTick() > m_timingInfo->getTick())
// 	{
// 		m_timingInfo = timingInfo;
// 	}
// }
