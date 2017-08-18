////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Authors: Trevor Sundberg, Joshua Claeys
/// Copyright 2017, DigiPen Institute of Technology
///
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Precompiled.hpp"

namespace Zero
{

namespace Events
{
DefineEvent(ScriptsCompiledPrePatch);
DefineEvent(ScriptsCompiledCommit);
DefineEvent(ScriptsCompiledPostPatch);
DefineEvent(ScriptCompilationFailed);
}//namespace Events

//------------------------------------------------------------------------------ Zilch Compile Event
ZilchDefineType(ZilchCompileEvent, builder, type)
{

}

//**************************************************************************************************
ZilchCompileEvent::ZilchCompileEvent(HashSet<ResourceLibrary*>& modifiedLibraries) :
  mModifiedLibraries(modifiedLibraries)
{

}

//**************************************************************************************************
bool ZilchCompileEvent::WasTypeModified(BoundType* type)
{
  forRange(ResourceLibrary* lib, mModifiedLibraries.All())
  {
    if (lib->BuiltType(type))
      return true;
  }

  return false;
}

//------------------------------------------------------------------------------------ Zilch Manager
//**************************************************************************************************
ZilchManager::ZilchManager() :
  mVersion(0),
  mShouldAttemptCompile(true),
  mLastCompileResult(CompileResult::CompilationSucceeded)
{
  ConnectThisTo(Z::gEngine, Events::EngineUpdate, OnEngineUpdate);
}

//**************************************************************************************************
void ZilchManager::TriggerCompileExternally()
{
  // Currently the version is used to detect duplicate errors
  // If something is externally triggering a compile (such as saving, project loading,
  // playing a game, etc) then we want to show duplicate errors again.
  ++mVersion;
  InternalCompile();
}

//**************************************************************************************************
void ZilchManager::InternalCompile()
{
  if (!mShouldAttemptCompile)
    return;
  mShouldAttemptCompile = false;

  forRange(ResourceLibrary* resourceLibrary, Z::gResources->LoadedResourceLibraries.Values())
  {
    if(resourceLibrary->CompileScripts(mPendingLibraries) == false)
    {
      Event eventToSend;
      this->DispatchEvent(Events::ScriptCompilationFailed, &eventToSend);
      mLastCompileResult = CompileResult::CompilationFailed;
      return;
    }
  }

  // If there are no pending libraries, nothing was compiled
  ErrorIf(mPendingLibraries.Empty(), "If the mShouldAttemptCompile flag was set, we should always have pending libraries (even at startup with no scripts)!");

  // Since we binary cache archetypes (in a way that is NOT saving the data tree, but rather a 'known serialization format'
  // then if we moved any properties around in any script it would completely destroy how the archetypes were cached
  // The simplest solution is to clear the cache
  ArchetypeManager::GetInstance()->FlushBinaryArchetypes();

  // Scripts were successfully compiled
  ZilchCompileEvent compileEvent(mPendingLibraries);

  this->DispatchEvent(Events::ScriptsCompiledPrePatch, &compileEvent);
  // Library commits must happen after all systems have handle PrePatch
  this->DispatchEvent(Events::ScriptsCompiledCommit, &compileEvent);

  forRange(ResourceLibrary* resourceLibrary, compileEvent.mModifiedLibraries.All())
    resourceLibrary->Commit();

  // @TrevorS: Refactor this to remove a global dependence on a single library.
  if(mPendingFragmentProjectLibrary)
  {
    mCurrentFragmentProjectLibrary = mPendingFragmentProjectLibrary;
    mPendingFragmentProjectLibrary = nullptr;
  }

  this->DispatchEvent(Events::ScriptsCompiledPostPatch, &compileEvent);

  MetaDatabase::GetInstance()->ClearRemovedLibraries();

  mPendingLibraries.Clear();

  mLastCompileResult = CompileResult::CompilationSucceeded;
}

//**************************************************************************************************
void ZilchManager::OnEngineUpdate(UpdateEvent* event)
{
  InternalCompile();
}

}//namespace Zero
