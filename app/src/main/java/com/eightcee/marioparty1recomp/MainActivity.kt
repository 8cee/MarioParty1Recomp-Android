package com.eightcee.marioparty1recomp

import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.eightcee.marioparty1recomp.diagnostics.Diagnostics

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Diagnostics.i("UI", "MainActivity created")
        val layout = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(48,48,48,48) }
        layout.addView(TextView(this).apply { text = "Mario Party 1 Recomp - Android\n\nInitial port scaffold"; textSize = 22f })
        layout.addView(Button(this).apply { text = "Write test diagnostic event"; setOnClickListener { Diagnostics.i("DIAG", "Manual test event from UI") } })
        layout.addView(Button(this).apply { text = "Export diagnostic log"; setOnClickListener { Diagnostics.exportLog(this@MainActivity) } })
        setContentView(layout)
    }
}
