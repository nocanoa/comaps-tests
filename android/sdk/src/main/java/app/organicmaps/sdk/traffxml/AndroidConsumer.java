/*
 * Copyright © 2017–2020 traffxml.org.
 * 
 * Relicensed to CoMaps by the original author.
 */

package app.organicmaps.sdk.traffxml;

import java.util.List;

import app.organicmaps.sdk.traffxml.Version;
import app.organicmaps.sdk.traffxml.AndroidTransport;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.IntentFilter.MalformedMimeTypeException;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;

public class AndroidConsumer {
	/**
	 * Creates an Intent filter which matches the Intents a TraFF consumer needs to receive.
	 * 
	 * <p>Different filters are available for consumers implementing different versions of the TraFF
	 * specification.
	 * 
	 * @param version The version of the TraFF specification (one of the constants in {@link org.traffxml.traff.Version})
	 * 
	 * @return An intent filter matching the necessary Intents
	 */
	public static IntentFilter createIntentFilter(int version) {
		IntentFilter res = new IntentFilter();
		switch (version) {
		case Version.V0_7:
			res.addAction(AndroidTransport.ACTION_TRAFF_PUSH);
			break;
		case Version.V0_8:
			res.addAction(AndroidTransport.ACTION_TRAFF_PUSH);
			res.addDataScheme(AndroidTransport.CONTENT_SCHEMA);
	        try {
	        	res.addDataType(AndroidTransport.MIME_TYPE_TRAFF);
	        } catch (MalformedMimeTypeException e) {
	        	// as long as the constant is a well-formed MIME type, this exception never gets thrown
	        	e.printStackTrace();
	        }
			break;
		default:
			throw new IllegalArgumentException("Invalid version code: " + version);
		}
		return res;
	}
	
	/**
	 * Sends a TraFF intent to a source.
	 * 
	 * <p>This encapsulates most of the low-level Android handling.
	 * 
	 * <p>If the recipient specified in {@code packageName} declares multiple receivers for the intent in its
	 * manifest, a separate intent will be delivered to each of them. The intent will not be delivered to
	 * receivers registered at runtime.
	 * 
	 * <p>All intents are sent as explicit ordered broadcasts. This means two things:
	 * 
	 * <p>Any app which declares a matching receiver in its manifest will be woken up to process the intent.
	 * This works even with certain Android 7 builds which restrict intent delivery to apps which are not
	 * currently running.
	 * 
	 * <p>It is safe for the recipient to unconditionally set result data. If the recipient does not set
	 * result data, the result will have a result code of
	 * {@link org.traffxml.transport.android.AndroidTransport#RESULT_INTERNAL_ERROR}, no data and no extras.
	 * 
	 * @param context The context
	 * @param action The intent action.
	 * @param data The intent data (for TraFF, this is the content provider URI), or null
	 * @param extras The extras for the intent
	 * @param packageName The package name for the intent recipient, or null to deliver the intent to all matching receivers
	 * @param receiverPermission A permission which the recipient must hold, or null if not required
	 * @param resultReceiver A BroadcastReceiver which will receive the result for the intent
	 */
	public static void sendTraffIntent(Context context, String action, Uri data, Bundle extras, String packageName,
			String receiverPermission, BroadcastReceiver resultReceiver) {
		Intent outIntent = new Intent(action);
		PackageManager pm = context.getPackageManager();
		List<ResolveInfo> receivers = pm.queryBroadcastReceivers(outIntent, 0);
		if (receivers != null)
			for (ResolveInfo receiver : receivers) {
				if ((packageName != null) && !packageName.equals(receiver.activityInfo.applicationInfo.packageName))
					continue;
				ComponentName cn = new ComponentName(receiver.activityInfo.applicationInfo.packageName,
						receiver.activityInfo.name);
				outIntent = new Intent(action);
				if (data != null)
					outIntent.setData(data);
				if (extras != null)
					outIntent.putExtras(extras);
				outIntent.setComponent(cn);
				context.sendOrderedBroadcast (outIntent, 
		                receiverPermission, 
		                resultReceiver, 
		                null, // scheduler, 
		                AndroidTransport.RESULT_INTERNAL_ERROR, // initialCode, 
		                null, // initialData, 
		                null);
			}
	}
}
