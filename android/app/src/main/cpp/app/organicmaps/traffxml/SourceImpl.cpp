// TODO which of the two do we need? (jni_helper includes jni)
//#include <jni>
#include "app/organicmaps/core/jni_helper.hpp"

#include "traffxml/traff_source.hpp"
#include "traffxml/traff_model_xml.hpp"

#include <optional>

extern "C"
{
  JNIEXPORT void JNICALL
  Java_app_organicmaps_traffxml_SourceImpl_onFeedReceivedImpl(JNIEnv * env, jclass thiz, jlong nativeManager, jstring feed)
  {
    std::string feedStd = jni::ToNativeString(env, feed);
    pugi::xml_document document;
    traffxml::TraffFeed parsedFeed;

    if (!document.load_string(feedStd.c_str()))
    {
      LOG(LWARNING, ("Feed is not a well-formed XML document"));
      return;
    }

    if (!traffxml::ParseTraff(document, std::nullopt, parsedFeed))
    {
      LOG(LWARNING, ("Feed is not a valid TraFF feed"));
      return;
    }

    traffxml::TraffSourceManager & manager = *reinterpret_cast<traffxml::TraffSourceManager*>(nativeManager);
    manager.ReceiveFeed(parsedFeed);
  }
}
