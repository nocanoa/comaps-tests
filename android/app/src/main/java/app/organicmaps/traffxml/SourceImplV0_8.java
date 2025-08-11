package app.organicmaps.traffxml;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.IntentFilter.MalformedMimeTypeException;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import app.organicmaps.util.log.Logger;

/**
 * Implementation for a TraFF 0.8 source.
 */
public class SourceImplV0_8 extends SourceImpl
{
  
  private String packageName;
  private String subscriptionId = null;

  /**
   * Creates a new instance.
   * 
   * @param context The application context
   * @param packageName The package name for the source
   */
  public SourceImplV0_8(Context context, long nativeManager, String packageName)
  {
    super(context, nativeManager);
    this.packageName = packageName;
  }

  /**
   * Subscribes to a traffic source.
   * 
   * @param filterList The filter list in XML format
   */
  @Override
  public void subscribe(String filterList)
  {
    IntentFilter filter = new IntentFilter();
    filter.addAction(AndroidTransport.ACTION_TRAFF_PUSH);
    filter.addDataScheme(AndroidTransport.CONTENT_SCHEMA);
    try
    {
      filter.addDataType(AndroidTransport.MIME_TYPE_TRAFF);
    }
    catch (MalformedMimeTypeException e)
    {
      // as long as the constant is a well-formed MIME type, this exception never gets thrown
      // TODO revisit logging
      e.printStackTrace();
    }

    context.registerReceiver(this, filter);
  }

  /**
   * Changes an existing traffic subscription.
   * 
   * @param filterList The filter list in XML format
   */
  @Override
  public void changeSubscription(String filterList)
  {
    Bundle extras = new Bundle();
    extras.putString(AndroidTransport.EXTRA_SUBSCRIPTION_ID, subscriptionId);
    extras.putString(AndroidTransport.EXTRA_FILTER_LIST, filterList);
    AndroidConsumer.sendTraffIntent(context, AndroidTransport.ACTION_TRAFF_SUBSCRIPTION_CHANGE, null,
        extras, packageName, Manifest.permission.ACCESS_COARSE_LOCATION, this);
  }

  /**
   * Unsubscribes from a traffic source we are subscribed to.
   */
  @Override
  public void unsubscribe()
  {
    Bundle extras = new Bundle();
    extras.putString(AndroidTransport.EXTRA_SUBSCRIPTION_ID, subscriptionId);
    AndroidConsumer.sendTraffIntent(this.context, AndroidTransport.ACTION_TRAFF_UNSUBSCRIBE, null,
        extras, packageName, Manifest.permission.ACCESS_COARSE_LOCATION, this);

    this.context.unregisterReceiver(this);
  }

  @Override
  public void onReceive(Context context, Intent intent)
  {
    if (intent == null)
      return;
    
    if (intent.getAction().equals(AndroidTransport.ACTION_TRAFF_PUSH))
    {
      Uri uri = intent.getData();
      if (uri != null)
      {
        /* 0.8 feed */
        String subscriptionId = intent.getStringExtra(AndroidTransport.EXTRA_SUBSCRIPTION_ID);
        if (subscriptionId.equals(this.subscriptionId))
          fetchMessages(context, uri);
      }
      else
      {
        Logger.w(this.getClass().getSimpleName(), "no URI in feed, ignoring");
      } // uri != null
  } else if (intent.getAction().equals(AndroidTransport.ACTION_TRAFF_SUBSCRIBE)) {
      if (this.getResultCode() != AndroidTransport.RESULT_OK) {
          Bundle extras = this.getResultExtras(true);
          if (extras != null)
            Logger.e(this.getClass().getSimpleName(), String.format("subscription to %s failed, %s",
                      extras.getString(AndroidTransport.EXTRA_PACKAGE), AndroidTransport.formatTraffError(this.getResultCode())));
          else
            Logger.e(this.getClass().getSimpleName(), String.format("subscription failed, %s",
                      AndroidTransport.formatTraffError(this.getResultCode())));
          return;
      }
      Bundle extras = this.getResultExtras(true);
      String data = this.getResultData();
      String packageName = extras.getString(AndroidTransport.EXTRA_PACKAGE);
      if (!this.packageName.equals(packageName))
        return;
      String subscriptionId = extras.getString(AndroidTransport.EXTRA_SUBSCRIPTION_ID);
      if (subscriptionId == null) {
        Logger.e(this.getClass().getSimpleName(),
                  String.format("subscription to %s failed: no subscription ID returned", packageName));
          return;
      } else if (packageName == null) {
        Logger.e(this.getClass().getSimpleName(), "subscription failed: no package name");
          return;
      } else if (data == null) {
        Logger.w(this.getClass().getSimpleName(),
                  String.format("subscription to %s successful (ID: %s) but no content URI was supplied. "
                          + "This is an issue with the source and may result in delayed message retrieval.",
                          packageName, subscriptionId));
          this.subscriptionId = subscriptionId;
          return;
      }
      Logger.d(this.getClass().getSimpleName(),
          "subscription to " + packageName + " successful, ID: " + subscriptionId);
      this.subscriptionId = subscriptionId;
      fetchMessages(context, Uri.parse(data));
  } else if (intent.getAction().equals(AndroidTransport.ACTION_TRAFF_SUBSCRIPTION_CHANGE)) {
      if (this.getResultCode() != AndroidTransport.RESULT_OK) {
          Bundle extras = this.getResultExtras(true);
          if (extras != null)
            Logger.e(this.getClass().getSimpleName(),
                      String.format("subscription change for %s failed: %s",
                              extras.getString(AndroidTransport.EXTRA_SUBSCRIPTION_ID),
                              AndroidTransport.formatTraffError(this.getResultCode())));
          else
            Logger.e(this.getClass().getSimpleName(),
                      String.format("subscription change failed: %s",
                              AndroidTransport.formatTraffError(this.getResultCode())));
          return;
      }
      Bundle extras = intent.getExtras();
      String data = this.getResultData();
      String subscriptionId = extras.getString(AndroidTransport.EXTRA_SUBSCRIPTION_ID);
      if (subscriptionId == null) {
        Logger.w(this.getClass().getSimpleName(),
                  "subscription change successful but the source did not specify the subscription ID. "
                          + "This is an issue with the source and may result in delayed message retrieval. "
                          + "URI: " + data);
          return;
      } else if (!subscriptionId.equals(this.subscriptionId)) {
        return;
      } else if (data == null) {
        Logger.w(this.getClass().getSimpleName(),
                  String.format("subscription change for %s successful but no content URI was supplied. "
                          + "This is an issue with the source and may result in delayed message retrieval.",
                          subscriptionId));
          return;
      }
      Logger.d(this.getClass().getSimpleName(),
          "subscription change for " + subscriptionId + " successful");
      fetchMessages(context, Uri.parse(data));
  } else if (intent.getAction().equals(AndroidTransport.ACTION_TRAFF_UNSUBSCRIBE)) {
    String subscriptionId = intent.getStringExtra(AndroidTransport.EXTRA_SUBSCRIPTION_ID);
    if (subscriptionId.equals(this.subscriptionId))
      this.subscriptionId = null;
    // TODO is there anything to do here? (Comment below is from Navit)
      /*
       * If we ever unsubscribe for reasons other than that we are shutting down or got a feed for
       * a subscription we don’t recognize, or if we start keeping a persistent list of
       * subscriptions, we need to delete the subscription from our list. Until then, there is
       * nothing to do here: either the subscription isn’t in the list, or we are about to shut
       * down and the whole list is about to get discarded.
       */
  } else if (intent.getAction().equals(AndroidTransport.ACTION_TRAFF_HEARTBEAT)) {
      String subscriptionId = intent.getStringExtra(AndroidTransport.EXTRA_SUBSCRIPTION_ID);
      if (subscriptionId.equals(this.subscriptionId)) {
        Logger.d(this.getClass().getSimpleName(),
                  String.format("got a heartbeat from %s for subscription %s; sending result",
                  intent.getStringExtra(AndroidTransport.EXTRA_PACKAGE), subscriptionId));
          this.setResult(AndroidTransport.RESULT_OK, null, null);
      }
  } // intent.getAction()
    // TODO Auto-generated method stub

  }

  /**
   * Fetches TraFF messages from a content provider.
   *
   * @param context The context to use for the content resolver
   * @param uri The content provider URI
   */
  private void fetchMessages(Context context, Uri uri) {
      try {
          Cursor cursor = context.getContentResolver().query(uri, new String[] {AndroidTransport.COLUMN_DATA}, null, null, null);
          if (cursor == null)
              return;
          if (cursor.getCount() < 1) {
              cursor.close();
              return;
          }
          StringBuilder builder = new StringBuilder("<feed>\n");
          while (cursor.moveToNext())
              builder.append(cursor.getString(cursor.getColumnIndex(AndroidTransport.COLUMN_DATA))).append("\n");
          builder.append("</feed>");
          cursor.close();
          onFeedReceived(builder.toString());
      } catch (Exception e) {
        Logger.w(this.getClass().getSimpleName(),
            String.format("Unable to fetch messages from %s", uri.toString()), e);
          e.printStackTrace();
      }
  }

}
