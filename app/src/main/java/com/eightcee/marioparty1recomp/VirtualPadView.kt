package com.eightcee.marioparty1recomp

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.view.MotionEvent
import android.view.View
import kotlin.math.hypot

class VirtualPadView(context: Context) : View(context) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { alpha = 150 }
    private var buttons = 0
    private var sx = 0f
    private var sy = 0f
    private val A=1 shl 0; private val B=1 shl 1; private val START=1 shl 2

    override fun onDraw(c: Canvas) {
        val h=height.toFloat(); val w=width.toFloat(); val r=h*.09f
        paint.style=Paint.Style.STROKE; paint.strokeWidth=6f
        c.drawCircle(w*.16f,h*.70f,r*1.45f,paint)
        c.drawCircle(w*.84f,h*.66f,r,paint); c.drawCircle(w*.72f,h*.78f,r*.82f,paint)
        c.drawRoundRect(w*.46f,h*.82f,w*.54f,h*.89f,20f,20f,paint)
        paint.style=Paint.Style.FILL
        c.drawText("A",w*.82f,h*.68f,paint); c.drawText("B",w*.70f,h*.80f,paint)
    }
    override fun onTouchEvent(e: MotionEvent): Boolean {
        buttons=0; sx=0f; sy=0f
        for(i in 0 until e.pointerCount) {
            if(e.actionMasked==MotionEvent.ACTION_UP || e.actionMasked==MotionEvent.ACTION_CANCEL) continue
            val x=e.getX(i); val y=e.getY(i); val w=width.toFloat(); val h=height.toFloat()
            if(x < w*.34f) { val dx=(x-w*.16f)/(h*.13f); val dy=(y-h*.70f)/(h*.13f); val d=hypot(dx,dy).coerceAtLeast(1f); sx=(dx/d).coerceIn(-1f,1f); sy=(-dy/d).coerceIn(-1f,1f) }
            if(hypot(x-w*.84f,y-h*.66f)<h*.13f) buttons=buttons or A
            if(hypot(x-w*.72f,y-h*.78f)<h*.12f) buttons=buttons or B
            if(x in w*.44f..w*.56f && y>h*.78f) buttons=buttons or START
        }
        RuntimeBridge.setVirtualPad(buttons,sx,sy); invalidate(); return true
    }
}
