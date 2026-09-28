.class public Lcom/sigmateam/sige/InputDeviceHelper;
.super Ljava/lang/Object;
.source "r8-map-id-b878e88416779bf451a79713f5d7af0c38dd553afbf12380a4ac004885ed32ef"


# static fields
.field private static final TAG:Ljava/lang/String; = "InputDeviceHelper"


# direct methods
.method public constructor <init>()V
    .registers 1

    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 2
    .line 3
    .line 4
    return-void
    .line 5
    .line 6
    .line 7
    .line 8
    .line 9
    .line 10
    .line 11
    .line 12
    .line 13
    .line 14
    .line 15
.end method

.method static getInputSources()I
    .registers 4

    .line 1
    invoke-static {}, Landroid/view/InputDevice;->getDeviceIds()[I

    .line 2
    .line 3
    .line 4
    move-result-object v0

    .line 5
    const/4 v1, 0x0

    .line 6
    move v2, v1

    .line 7
    :goto_6
    array-length v3, v0

    .line 8
    if-ge v1, v3, :cond_1b

    .line 9
    .line 10
    aget v3, v0, v1

    .line 11
    .line 12
    invoke-static {v3}, Landroid/view/InputDevice;->getDevice(I)Landroid/view/InputDevice;

    .line 13
    .line 14
    .line 15
    move-result-object v3

    .line 16
    if-eqz v3, :cond_18

    .line 17
    .line 18
    invoke-virtual {v3}, Landroid/view/InputDevice;->getSources()I

    .line 19
    .line 20
    .line 21
    move-result v3

    .line 22
    if-lez v3, :cond_18

    .line 23
    .line 24
    or-int/2addr v2, v3

    .line 25
    :cond_18
    add-int/lit8 v1, v1, 0x1

    .line 26
    .line 27
    goto :goto_6

    .line 28
    :cond_1b
    return v2
    .line 29
    .line 30
    .line 31
    .line 32
    .line 33
    .line 34
    .line 35
    .line 36
    .line 37
    .line 38
    .line 39
    .line 40
    .line 41
    .line 42
    .line 43
    .line 44
    .line 45
    .line 46
    .line 47
    .line 48
    .line 49
    .line 50
    .line 51
    .line 52
    .line 53
    .line 54
    .line 55
    .line 56
    .line 57
    .line 58
    .line 59
    .line 60
    .line 61
    .line 62
    .line 63
    .line 64
    .line 65
    .line 66
.end method

.method static getMotionRanges(Landroid/view/InputDevice;)V
    .registers 6

    .line 1
    if-eqz p0, :cond_36

    .line 2
    .line 3
    invoke-virtual {p0}, Landroid/view/InputDevice;->getMotionRanges()Ljava/util/List;

    .line 4
    .line 5
    .line 6
    move-result-object p0

    .line 7
    const/4 v0, 0x0

    .line 8
    :goto_7
    invoke-interface {p0}, Ljava/util/List;->size()I

    .line 9
    .line 10
    .line 11
    move-result v1

    .line 12
    if-ge v0, v1, :cond_36

    .line 13
    .line 14
    invoke-interface {p0, v0}, Ljava/util/List;->get(I)Ljava/lang/Object;

    .line 15
    .line 16
    .line 17
    move-result-object v1

    .line 18
    check-cast v1, Landroid/view/InputDevice$MotionRange;

    .line 19
    .line 20
    invoke-virtual {v1}, Landroid/view/InputDevice$MotionRange;->getFlat()F

    .line 21
    .line 22
    .line 23
    move-result v2

    .line 24
    invoke-virtual {v1}, Landroid/view/InputDevice$MotionRange;->getFuzz()F

    .line 25
    .line 26
    .line 27
    move-result v3

    .line 28
    add-float/2addr v2, v3

    .line 29
    const v3, 0x3dcccccd    # 0.1f

    .line 30
    .line 31
    .line 32
    cmpg-float v4, v2, v3

    .line 33
    .line 34
    if-gez v4, :cond_24

    .line 35
    .line 36
    move v2, v3

    .line 37
    :cond_24
    invoke-virtual {v1}, Landroid/view/InputDevice$MotionRange;->getAxis()I

    .line 38
    .line 39
    .line 40
    move-result v3

    .line 41
    invoke-virtual {v1}, Landroid/view/InputDevice$MotionRange;->getMin()F

    .line 42
    .line 43
    .line 44
    move-result v4

    .line 45
    invoke-virtual {v1}, Landroid/view/InputDevice$MotionRange;->getMax()F

    .line 46
    .line 47
    .line 48
    move-result v1

    .line 49
    invoke-static {v3, v4, v1, v2}, Lcom/sigmateam/sige/InputDeviceHelper;->onAxisInfo(IFFF)V

    .line 50
    .line 51
    .line 52
    add-int/lit8 v0, v0, 0x1

    .line 53
    .line 54
    goto :goto_7

    .line 55
    :cond_36
    return-void
    .line 56
    .line 57
    .line 58
    .line 59
    .line 60
    .line 61
    .line 62
    .line 63
    .line 64
    .line 65
    .line 66
    .line 67
    .line 68
    .line 69
    .line 70
    .line 71
    .line 72
    .line 73
    .line 74
    .line 75
    .line 76
    .line 77
    .line 78
    .line 79
    .line 80
    .line 81
    .line 82
    .line 83
    .line 84
    .line 85
    .line 86
    .line 87
    .line 88
    .line 89
    .line 90
    .line 91
    .line 92
    .line 93
    .line 94
    .line 95
    .line 96
    .line 97
    .line 98
    .line 99
    .line 100
    .line 101
    .line 102
    .line 103
    .line 104
    .line 105
.end method

.method private static native onAxisInfo(IFFF)V
.end method
