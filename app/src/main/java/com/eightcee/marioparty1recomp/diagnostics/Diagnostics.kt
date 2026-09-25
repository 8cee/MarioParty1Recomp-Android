package com.eightcee.marioparty1recomp.diagnostics

import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import androidx.core.content.FileProvider
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object Diagnostics {
    private lateinit var logFile: File
    private val lock = Any()
    private val formatter = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)
    fun init(context: Context) {
        val dir = File(context.filesDir, "diagnostics"); dir.mkdirs(); logFile = File(dir, "marioparty1.log")
        i("SYSTEM", "device=" + Build.MANUFACTURER + " " + Build.MODEL)
        i("SYSTEM", "android=" + Build.VERSION.RELEASE + " sdk=" + Build.VERSION.SDK_INT)
        i("SYSTEM", "abi=" + Build.SUPPORTED_ABIS.joinToString())
        installCrashHandler()
    }
    fun i(tag:String,message:String)=write("INFO",tag,message)
    fun w(tag:String,message:String)=write("WARN",tag,message)
    fun e(tag:String,message:String,t:Throwable?=null){ write("ERROR",tag,message); t?.let{write("ERROR",tag,it.stackTraceToString())} }
    private fun write(level:String,tag:String,message:String){
        if(!::logFile.isInitialized)return
        val line=formatter.format(Date())+" ["+level+"] ["+tag+"] "+message+"\n"
        synchronized(lock){
            logFile.appendText(line)
            if(logFile.length()>2_000_000){ val backup=File(logFile.parentFile,"marioparty1.previous.log"); if(backup.exists())backup.delete(); logFile.renameTo(backup); logFile=File(logFile.parentFile,"marioparty1.log") }
        }
    }
    private fun installCrashHandler(){ val previous=Thread.getDefaultUncaughtExceptionHandler(); Thread.setDefaultUncaughtExceptionHandler{thread,t->e("CRASH","Uncaught exception on thread="+thread.name,t); previous?.uncaughtException(thread,t)} }
    fun exportLog(context:Context){
        if(!::logFile.isInitialized||!logFile.exists())return
        val uri=FileProvider.getUriForFile(context,context.packageName+".fileprovider",logFile)
        val intent=Intent(Intent.ACTION_SEND).apply{type="text/plain";putExtra(Intent.EXTRA_STREAM,uri);addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)}
        context.startActivity(Intent.createChooser(intent,"Export diagnostic log"))
    }
}
