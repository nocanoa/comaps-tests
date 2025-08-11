package app.organicmaps.traffxml;

import java.util.ArrayList;
import java.util.List;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import app.organicmaps.util.log.Logger;

/**
 * Implementation for a TraFF 0.7 source.
 */
public class SourceImplV0_7 extends SourceImpl
{
  private PackageManager pm;

  /**
   * Creates a new instance.
   * 
   * @param context The application context
   */
  public SourceImplV0_7(Context context, long nativeManager)
  {
    super(context, nativeManager);
    // TODO Auto-generated constructor stub
  }

  /**
   * Subscribes to a traffic source.
   * 
   * @param filterList The filter list in XML format
   */
  @Override
  public void subscribe(String filterList)
  {
    IntentFilter traffFilter07 = new IntentFilter();
    traffFilter07.addAction(AndroidTransport.ACTION_TRAFF_PUSH);

    this.context.registerReceiver(this, traffFilter07);

    // Broadcast a poll intent to all TraFF 0.7-only receivers
    Intent outIntent = new Intent(AndroidTransport.ACTION_TRAFF_POLL);
    pm = this.context.getPackageManager();
    List<ResolveInfo> receivers07 = pm.queryBroadcastReceivers(outIntent, 0);
    List<ResolveInfo> receivers08 = pm.queryBroadcastReceivers(new Intent(AndroidTransport.ACTION_TRAFF_GET_CAPABILITIES), 0);
    if (receivers07 != null)
    {
      /*
       * Get receivers which support only TraFF 0.7 and poll them.
       * If there are no TraFF 0.7 sources at the moment, we register the receiver nonetheless.
       * That way, if any new sources are added during the session, we get any messages they send.
       */
      if (receivers08 != null)
        receivers07.removeAll(receivers08);
      for (ResolveInfo receiver : receivers07)
      {
        ComponentName cn = new ComponentName(receiver.activityInfo.applicationInfo.packageName,
            receiver.activityInfo.name);
        outIntent = new Intent(AndroidTransport.ACTION_TRAFF_POLL);
        outIntent.setComponent(cn);
        this.context.sendBroadcast(outIntent, Manifest.permission.ACCESS_COARSE_LOCATION);
      }
    }
  }

  /**
   * Changes an existing traffic subscription.
   * 
   * This implementation does nothing, as TraFF 0.7 does not support subscriptions.
   *
   * @param filterList The filter list in XML format
   */
  @Override
  public void changeSubscription(String filterList)
  {
    // NOP
  }

  /**
   * Unsubscribes from a traffic source we are subscribed to.
   */
  @Override
  public void unsubscribe()
  {
    this.context.unregisterReceiver(this);
  }

  @Override
  public void onReceive(Context context, Intent intent)
  {
    if (intent == null)
      return;

    if (intent.getAction().equals(AndroidTransport.ACTION_TRAFF_PUSH))
    {
      /* 0.7 feed */
      String packageName = intent.getStringExtra(AndroidTransport.EXTRA_PACKAGE);
      /*
       * If the feed comes from a TraFF 0.8+ source, skip it (this may happen with “bilingual”
       * TraFF 0.7/0.8 sources). That ensures the only way to get information from such sources is
       * through a TraFF 0.8 subscription. Fetching the list from scratch each time ensures that
       * apps installed during runtime get considered.)  
       */
      if (packageName != null)
      {
        for (ResolveInfo info : pm.queryBroadcastReceivers(new Intent(AndroidTransport.ACTION_TRAFF_GET_CAPABILITIES), 0))
          if (packageName.equals(info.resolvePackageName))
            return;
      }
      String feed = intent.getStringExtra(AndroidTransport.EXTRA_FEED);
      if (feed == null)
      {
        Logger.w(this.getClass().getSimpleName(), "empty feed, ignoring");
      }
      else
      {
        onFeedReceived(feed);
      }
    }
  }

}
