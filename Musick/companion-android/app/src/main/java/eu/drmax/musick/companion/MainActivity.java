package eu.drmax.musick.companion;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Minimal UI: the companion does its work in the background service. The only
 * thing the user needs to do here is grant "Notification access", which is what
 * lets the service read the phone's media sessions.
 */
public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button grant = findViewById(R.id.grant);
        grant.setOnClickListener(v ->
                startActivity(new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)));
    }
}
