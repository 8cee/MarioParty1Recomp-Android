package com.eightcee.marioparty1recomp

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min

/**
 * DK64-style translucent N64 touch pad.
 *
 * The native bridge ORs [buttons] directly into N64ModernRuntime's N64 button
 * mask, so these values MUST be the real libultra/controller bit values.
 */
class VirtualPadView(context: Context) : View(context) {
    companion object {
        private const val BTN_A = 0x8000
        private const val BTN_B = 0x4000
        private const val BTN_Z = 0x2000
        private const val BTN_START = 0x1000
        private const val BTN_D_UP = 0x0800
        private const val BTN_D_DOWN = 0x0400
        private const val BTN_D_LEFT = 0x0200
        private const val BTN_D_RIGHT = 0x0100
        private const val BTN_L = 0x0020
        private const val BTN_R = 0x0010
        private const val BTN_C_UP = 0x0008
        private const val BTN_C_DOWN = 0x0004
        private const val BTN_C_LEFT = 0x0002
        private const val BTN_C_RIGHT = 0x0001

        private const val STICK_DEADZONE = 0.08f
    }

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.rgb(10, 13, 18)
        alpha = 145
    }
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
        color = Color.WHITE
        alpha = 150
    }
    private val label = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        alpha = 220
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }
    private val accent = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 5f
        alpha = 215
    }

    private var buttons = 0
    private var sx = 0f
    private var sy = 0f

    private var unit = 1f
    private var stickX = 0f
    private var stickY = 0f
    private var stickR = 0f
    private var knobR = 0f

    private data class RoundButton(
        val mask: Int,
        val text: String,
        val color: Int,
        var x: Float = 0f,
        var y: Float = 0f,
        var r: Float = 0f
    )

    private data class PillButton(
        val mask: Int,
        val text: String,
        var rect: RectF = RectF()
    )

    private val a = RoundButton(BTN_A, "A", Color.rgb(90, 150, 230))
    private val b = RoundButton(BTN_B, "B", Color.rgb(80, 190, 95))
    private val cu = RoundButton(BTN_C_UP, "▲", Color.rgb(245, 214, 80))
    private val cd = RoundButton(BTN_C_DOWN, "▼", Color.rgb(245, 214, 80))
    private val cl = RoundButton(BTN_C_LEFT, "◀", Color.rgb(245, 214, 80))
    private val cr = RoundButton(BTN_C_RIGHT, "▶", Color.rgb(245, 214, 80))
    private val start = RoundButton(BTN_START, "START", Color.rgb(225, 85, 95))

    private val dUp = RoundButton(BTN_D_UP, "▲", Color.LTGRAY)
    private val dDown = RoundButton(BTN_D_DOWN, "▼", Color.LTGRAY)
    private val dLeft = RoundButton(BTN_D_LEFT, "◀", Color.LTGRAY)
    private val dRight = RoundButton(BTN_D_RIGHT, "▶", Color.LTGRAY)

    private val roundButtons = arrayOf(a, b, cu, cd, cl, cr, start, dUp, dDown, dLeft, dRight)

    private val z = PillButton(BTN_Z, "Z")
    private val l = PillButton(BTN_L, "L")
    private val r = PillButton(BTN_R, "R")
    private val pillButtons = arrayOf(z, l, r)

    init {
        isFocusable = false
        isClickable = false
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        val fw = w.toFloat()
        val fh = h.toFloat()
        unit = min(fw / 1280f, fh / 720f).coerceAtLeast(0.55f)

        stickX = fw * 0.14f
        stickY = fh * 0.68f
        stickR = 92f * unit
        knobR = 42f * unit

        fun rb(btn: RoundButton, x: Float, y: Float, radius: Float) {
            btn.x = fw * x
            btn.y = fh * y
            btn.r = radius * unit
        }

        rb(a, 0.88f, 0.60f, 53f)
        rb(b, 0.78f, 0.75f, 39f)

        val ccx = fw * 0.76f
        val ccy = fh * 0.39f
        val csx = 78f * unit
        val csy = 62f * unit
        val crad = 31f * unit
        cu.x = ccx; cu.y = ccy - csy; cu.r = crad
        cd.x = ccx; cd.y = ccy + csy; cd.r = crad
        cl.x = ccx - csx; cl.y = ccy; cl.r = crad
        cr.x = ccx + csx; cr.y = ccy; cr.r = crad

        rb(start, 0.49f, 0.87f, 35f)

        // Compact D-pad under the analog stick. Mario Party mainly uses the
        // analog stick, but keeping the real D-pad makes menus/minigames safe.
        val dx = fw * 0.34f
        val dy = fh * 0.80f
        val ds = 43f * unit
        val dgap = 45f * unit
        dUp.x = dx; dUp.y = dy - dgap; dUp.r = ds
        dDown.x = dx; dDown.y = dy + dgap; dDown.r = ds
        dLeft.x = dx - dgap; dLeft.y = dy; dLeft.r = ds
        dRight.x = dx + dgap; dRight.y = dy; dRight.r = ds

        val trigW = 170f * unit
        val trigH = 66f * unit
        val top = 24f * unit
        z.rect = RectF(28f * unit, top, 28f * unit + trigW, top + trigH)
        l.rect = RectF(fw * 0.5f - trigW / 2f, top, fw * 0.5f + trigW / 2f, top + trigH)
        r.rect = RectF(fw - 28f * unit - trigW, top, fw - 28f * unit, top + trigH)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        // Analog base + knob.
        fill.color = Color.rgb(10, 13, 18)
        fill.alpha = 145
        stroke.color = Color.WHITE
        stroke.alpha = 125
        stroke.strokeWidth = 4f * unit
        canvas.drawCircle(stickX, stickY, stickR, fill)
        canvas.drawCircle(stickX, stickY, stickR, stroke)

        val travel = max(0f, stickR - knobR - 6f * unit)
        canvas.drawCircle(stickX + sx * travel, stickY - sy * travel, knobR, fill)
        accent.color = Color.rgb(155, 211, 43)
        accent.strokeWidth = 4f * unit
        canvas.drawCircle(stickX + sx * travel, stickY - sy * travel, knobR, accent)

        for (p in pillButtons) {
            val pressed = (buttons and p.mask) != 0
            fill.alpha = if (pressed) 205 else 145
            stroke.alpha = if (pressed) 245 else 145
            canvas.drawRoundRect(p.rect, p.rect.height() / 2f, p.rect.height() / 2f, fill)
            canvas.drawRoundRect(p.rect, p.rect.height() / 2f, p.rect.height() / 2f, stroke)
            label.textSize = 28f * unit
            label.alpha = 230
            canvas.drawText(p.text, p.rect.centerX(), p.rect.centerY() + label.textSize * 0.34f, label)
        }

        for (btn in roundButtons) {
            val pressed = (buttons and btn.mask) != 0
            fill.alpha = if (pressed) 215 else 145
            stroke.color = btn.color
            stroke.alpha = if (pressed) 255 else 180
            stroke.strokeWidth = if (pressed) 6f * unit else 4f * unit
            canvas.drawCircle(btn.x, btn.y, btn.r, fill)
            canvas.drawCircle(btn.x, btn.y, btn.r, stroke)

            label.color = btn.color
            label.alpha = 235
            label.textSize = when {
                btn === start -> 16f * unit
                btn.text.length > 1 -> 18f * unit
                else -> 28f * unit
            }
            canvas.drawText(btn.text, btn.x, btn.y + label.textSize * 0.34f, label)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val liftedIndex = when (event.actionMasked) {
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> event.actionIndex
            else -> -1
        }

        var nextButtons = 0
        var nextX = 0f
        var nextY = 0f
        var stickClaimed = false

        if (event.actionMasked != MotionEvent.ACTION_CANCEL) {
            for (i in 0 until event.pointerCount) {
                if (i == liftedIndex) continue

                val x = event.getX(i)
                val y = event.getY(i)

                // Stick gets first claim on the left side so an analog drag
                // cannot accidentally hit the nearby D-pad.
                if (!stickClaimed && hitCircle(x, y, stickX, stickY, stickR * 1.45f)) {
                    var dx = (x - stickX) / stickR
                    var dy = -(y - stickY) / stickR
                    val mag = hypot(dx, dy)
                    if (mag > 1f) {
                        dx /= mag
                        dy /= mag
                    }
                    if (mag <= STICK_DEADZONE) {
                        dx = 0f
                        dy = 0f
                    } else {
                        val clipped = min(mag, 1f)
                        val scaled = (clipped - STICK_DEADZONE) / (1f - STICK_DEADZONE)
                        if (clipped > 0f) {
                            dx = dx / clipped * scaled
                            dy = dy / clipped * scaled
                        }
                    }
                    nextX = dx.coerceIn(-1f, 1f)
                    nextY = dy.coerceIn(-1f, 1f)
                    stickClaimed = true
                    continue
                }

                for (p in pillButtons) {
                    val expanded = RectF(p.rect).apply { inset(-14f * unit, -14f * unit) }
                    if (expanded.contains(x, y)) nextButtons = nextButtons or p.mask
                }

                for (btn in roundButtons) {
                    val hitR = max(btn.r * 1.42f, 44f * unit)
                    if (hitCircle(x, y, btn.x, btn.y, hitR)) nextButtons = nextButtons or btn.mask
                }
            }
        }

        buttons = nextButtons
        sx = nextX
        sy = nextY
        RuntimeBridge.setVirtualPad(buttons, sx, sy)
        invalidate()
        return true
    }

    override fun onWindowFocusChanged(hasWindowFocus: Boolean) {
        super.onWindowFocusChanged(hasWindowFocus)
        if (!hasWindowFocus) releaseAll()
    }

    override fun onDetachedFromWindow() {
        releaseAll()
        super.onDetachedFromWindow()
    }

    private fun releaseAll() {
        buttons = 0
        sx = 0f
        sy = 0f
        RuntimeBridge.setVirtualPad(0, 0f, 0f)
        invalidate()
    }

    private fun hitCircle(x: Float, y: Float, cx: Float, cy: Float, radius: Float): Boolean {
        val dx = x - cx
        val dy = y - cy
        return dx * dx + dy * dy <= radius * radius
    }
}
