FROM debian:bookworm-slim
RUN apt-get update \
    && apt-get install -y --no-install-recommends libopengl0 libstdc++6 python3 \
    && rm -rf /var/lib/apt/lists/*
COPY compiled/ /opt/gdp/
COPY source/assets/ai/ /opt/gdp/assets/ai/
COPY source/SOURCE_COMMIT /opt/gdp/SOURCE_COMMIT
COPY train_service.py /opt/gdp/train_service.py
COPY training_evaluation.py /opt/gdp/training_evaluation.py
COPY compare_population.py evaluate_final.py /opt/gdp/
ENV LD_LIBRARY_PATH=/opt/gdp
WORKDIR /opt/gdp
ENTRYPOINT ["python3", "/opt/gdp/train_service.py"]
